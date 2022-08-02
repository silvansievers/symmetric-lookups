#include "search_space.h"

#include "search_node_info.h"
#include "task_proxy.h"

#include "structural_symmetries/group.h"
#include "structural_symmetries/permutation.h"

#include "task_utils/successor_generator.h"
#include "task_utils/task_properties.h"

#include "utils/logging.h"

#include <cassert>

using namespace std;

SearchNode::SearchNode(const State &state, SearchNodeInfo &info)
    : state(state), info(info) {
    assert(state.get_id() != StateID::no_state);
}

const State &SearchNode::get_state() const {
    return state;
}

bool SearchNode::is_open() const {
    return info.status == SearchNodeInfo::OPEN;
}

bool SearchNode::is_closed() const {
    return info.status == SearchNodeInfo::CLOSED;
}

bool SearchNode::is_dead_end() const {
    return info.status == SearchNodeInfo::DEAD_END;
}

bool SearchNode::is_new() const {
    return info.status == SearchNodeInfo::NEW;
}

int SearchNode::get_g() const {
    assert(info.g >= 0);
    return info.g;
}

int SearchNode::get_real_g() const {
    return info.real_g;
}

void SearchNode::open_initial() {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = 0;
    info.real_g = 0;
    info.parent_state_id = StateID::no_state;
    info.creating_operator = OperatorID::no_operator;
}

void SearchNode::open(const SearchNode &parent_node,
                      const OperatorProxy &parent_op,
                      int adjusted_cost) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + adjusted_cost;
    info.real_g = parent_node.info.real_g + parent_op.get_cost();
    info.parent_state_id = parent_node.get_state().get_id();
    info.creating_operator = OperatorID(parent_op.get_id());
}

void SearchNode::reopen(const SearchNode &parent_node,
                        const OperatorProxy &parent_op,
                        int adjusted_cost) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);

    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + adjusted_cost;
    info.real_g = parent_node.info.real_g + parent_op.get_cost();
    info.parent_state_id = parent_node.get_state().get_id();
    info.creating_operator = OperatorID(parent_op.get_id());
}

// like reopen, except doesn't change status
void SearchNode::update_parent(const SearchNode &parent_node,
                               const OperatorProxy &parent_op,
                               int adjusted_cost) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);
    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.g = parent_node.info.g + adjusted_cost;
    info.real_g = parent_node.info.real_g + parent_op.get_cost();
    info.parent_state_id = parent_node.get_state().get_id();
    info.creating_operator = OperatorID(parent_op.get_id());
}

void SearchNode::close() {
    assert(info.status == SearchNodeInfo::OPEN);
    info.status = SearchNodeInfo::CLOSED;
}

void SearchNode::mark_as_dead_end() {
    info.status = SearchNodeInfo::DEAD_END;
}

void SearchNode::dump(const TaskProxy &task_proxy, utils::LogProxy &log) const {
    if (log.is_at_least_debug()) {
        log << state.get_id() << ": ";
        task_properties::dump_fdr(state);
        if (info.creating_operator != OperatorID::no_operator) {
            OperatorsProxy operators = task_proxy.get_operators();
            OperatorProxy op = operators[info.creating_operator.get_index()];
            log << " created by " << op.get_name()
                << " from " << info.parent_state_id << endl;
        } else {
            log << " no parent" << endl;
        }
    }
}

SearchSpace::SearchSpace(StateRegistry &state_registry, utils::LogProxy &log)
    : state_registry(state_registry), log(log) {
}

SearchNode SearchSpace::get_node(const State &state) {
    return SearchNode(state, search_node_infos[state]);
}

void SearchSpace::trace_path(const State &goal_state,
                             vector<OperatorID> &path,
                             const shared_ptr<AbstractTask> &task,
                             const shared_ptr<Group> &group) const {
    if (group && group->has_symmetries()) {
        trace_path_with_symmetries(goal_state, path, task, group);
        return;
    }
    State current_state = goal_state;
    assert(current_state.get_registry() == &state_registry);
    assert(path.empty());
    for (;;) {
        const SearchNodeInfo &info = search_node_infos[current_state];
        if (info.creating_operator == OperatorID::no_operator) {
            assert(info.parent_state_id == StateID::no_state);
            break;
        }
        path.push_back(info.creating_operator);
        current_state = state_registry.lookup_state(info.parent_state_id);
    }
    reverse(path.begin(), path.end());
}

void SearchSpace::trace_path_with_symmetries(const State &goal_state,
                                             vector<OperatorID> &path,
                                             const shared_ptr<AbstractTask> &task,
                                             const shared_ptr<Group> &group) const {
    assert(path.empty());
    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();

    /*
      For DKS, we need to use a separate registry to generate successor states
      to avoid generating the symmetrical successor state, which could equal
      the current_state of the state trace.

      For OSS; we can use the regular registry, as it works directly on
      canonical representatives.
    */
    StateRegistry dks_successor_state_registry(task_proxy);

    StateRegistry *successor_registry =
        group->get_search_symmetries() == SearchSymmetries::DKS ?
                &dks_successor_state_registry : &state_registry;

    vector<RawPermutation> permutations;
    vector<State> state_trace;
    State current_state = goal_state;
    while (true) {
        const SearchNodeInfo &info = search_node_infos[current_state];
        assert(info.status != SearchNodeInfo::NEW);
        OperatorID op_id = info.creating_operator;
        state_trace.push_back(current_state);
        // Important: new_state needs to be the initial state!
        State parent_state = state_registry.get_initial_state();
        State new_state = state_registry.get_initial_state();
        if (op_id != OperatorID::no_operator) {
            parent_state = state_registry.lookup_state(info.parent_state_id);
            new_state = successor_registry->get_successor_state(parent_state, operators[op_id]);
        }
        RawPermutation p;
        if (new_state.get_id() != current_state.get_id()){
            p = group->create_permutation_from_state_to_state(current_state, new_state);
        } else {
            p = group->new_identity_raw_permutation();
        }
        permutations.push_back(move(p));
        if (op_id == OperatorID::no_operator)
            break;
        current_state = parent_state;
    }
    assert(state_trace.size() == permutations.size());
    vector<RawPermutation> reverse_permutations;
    RawPermutation temp_p = group->new_identity_raw_permutation();
    while (permutations.begin() != permutations.end()) {
        const RawPermutation &p = permutations.back();
        temp_p = group->compose_permutations(p, temp_p);
        reverse_permutations.push_back(temp_p);
        permutations.pop_back();
    }
    for (size_t i = 0; i < state_trace.size(); ++i){
        const RawPermutation &permutation = reverse_permutations[state_trace.size() - i-1];
        state_trace[i] = successor_registry->permute_state(state_trace[i],
                                                           Permutation(*group, permutation));
    }
    reverse_permutations.clear();
    for (int i = state_trace.size() - 1; i > 0; i--) {
        vector<OperatorID> applicable_ops;
        successor_generator::g_successor_generators[task_proxy].generate_applicable_ops(state_trace[i], applicable_ops);
        bool found = false;
        int min_cost_op=0;
        int min_cost=numeric_limits<int>::max();

        for (size_t o = 0; o < applicable_ops.size(); o++) {
            OperatorProxy op = operators[applicable_ops[o]];
            State succ_state = successor_registry->get_successor_state(state_trace[i], op);
            if (succ_state.get_id() == state_trace[i-1].get_id()) {
                found = true;
                if (op.get_cost() < min_cost) {
                    min_cost = op.get_cost();
                    min_cost_op = o;
                }
            }
        }
        if (!found) {
            utils::g_log << "No operator is found!!!" << endl
                 << "Cannot reach the state " << endl;
            task_properties::dump_pddl(state_trace[i-1]);
            utils::g_log << endl << "From the state" << endl;
            task_properties::dump_pddl(state_trace[i]);
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
        path.push_back(applicable_ops[min_cost_op]);
    }
}

void SearchSpace::dump(const TaskProxy &task_proxy) const {
    OperatorsProxy operators = task_proxy.get_operators();
    for (StateID id : state_registry) {
        /* The body duplicates SearchNode::dump() but we cannot create
           a search node without discarding the const qualifier. */
        State state = state_registry.lookup_state(id);
        const SearchNodeInfo &node_info = search_node_infos[state];
        log << id << ": ";
        task_properties::dump_fdr(state);
        if (node_info.creating_operator != OperatorID::no_operator &&
            node_info.parent_state_id != StateID::no_state) {
            OperatorProxy op = operators[node_info.creating_operator.get_index()];
            log << " created by " << op.get_name()
                << " from " << node_info.parent_state_id << endl;
        } else {
            log << "has no parent" << endl;
        }
    }
}

void SearchSpace::print_statistics() const {
    state_registry.print_statistics(log);
}
