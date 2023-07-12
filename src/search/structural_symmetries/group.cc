#include "group.h"

#include "graph_creator.h"
#include "permutation.h"

#include "../per_state_information.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../utils/memory.h"
#include "../tasks/root_task.h"

#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <queue>


using namespace std;
using namespace utils;

Group::Group(const plugins::Options &opts)
    : stabilize_initial_state(opts.get<bool>("stabilize_initial_state")),
      stabilize_goal(opts.get<bool>("stabilize_goal")),
      use_color_for_stabilizing_goal(opts.get<bool>("use_color_for_stabilizing_goal")),
      time_bound(opts.get<int>("time_bound")),
      dump_symmetry_graph(opts.get<bool>("dump_symmetry_graph")),
      search_symmetries(opts.get<SearchSymmetries>("search_symmetries")),
      symmetrical_lookups(opts.get<SymmetricalLookups>("symmetrical_lookups")),
      rw_length_or_number_symmetric_states(opts.get<int>("symmetry_rw_length_or_number_states")),
      rng(utils::parse_rng_from_options(opts)),
      dump_permutations(opts.get<bool>("dump_permutations")),
      write_search_generators(opts.get<bool>("write_search_generators")),
      write_all_generators(opts.get<bool>("write_all_generators")),
      num_vars(0),
      permutation_length(0),
      graph_size(0),
      num_identity_generators(0),
      initialized(false) {
}

const Permutation &Group::get_permutation(int index) const {
    return generators[index];
}

void Group::add_to_dom_sum_by_var(int summed_dom) {
    dom_sum_by_var.push_back(summed_dom);
}

void Group::add_to_var_by_val(int var) {
    var_by_val.push_back(var);
}

void Group::compute_symmetries(const TaskProxy &task_proxy) {
    if (initialized || !generators.empty()) {
        cerr << "Already computed symmetries" << endl;
        exit_with(ExitCode::SEARCH_CRITICAL_ERROR);
    }
    GraphCreator graph_creator;
    bool success = graph_creator.compute_symmetries(
        task_proxy,
        stabilize_initial_state,
        stabilize_goal,
        use_color_for_stabilizing_goal,
        time_bound,
        dump_symmetry_graph,
        this);
    if (!success) {
        generators.clear();
    }
    // Set initialized to true regardless of whether symmetries have been
    // found or not to avoid future attempts at computing symmetries if
    // none can be found.
    initialized = true;

    if (write_search_generators || write_all_generators) {
        write_generators();
        utils::exit_with(utils::ExitCode::SUCCESS);
    }
}

void Group::write_generators() const {
    assert(write_search_generators || write_all_generators);

    /*
      To avoid writing large generators, we first compute the set of vertices
      that is actually affected by any generator and assign them consecutive
      numbers.
    */
    unordered_map<int, int> vertex_to_id;
    int vertex_counter = 0;
    for (const auto &generator : to_be_written_generators) {
        for (const pair<const int, int> &key_val : generator) {
            if (!vertex_to_id.count(key_val.first)) {
                vertex_to_id[key_val.first] = vertex_counter++;
            }
        }
    }

    /*
      Then we go over all generators again, writing them out as permutations
      (python-style lists) using the vertex-to-id mapping.
    */
    ofstream file;
    file.open("generators.py", std::ios_base::out);
    for (const auto &generator : to_be_written_generators) {
        vector<int> permutation(vertex_counter);
        iota(permutation.begin(), permutation.end(), 0);
        for (const pair<const int, int> &key_val : generator) {
            permutation[vertex_to_id[key_val.first]] = vertex_to_id[key_val.second];
        }
        file << "[";
        for (size_t i = 0; i < permutation.size(); ++i) {
            file << permutation[i];
            if (i != permutation.size() - 1) {
                file << ", ";
            }
        }
        file << "]" << endl;
    }
    file.close();
}

void Group::add_to_be_written_generator(const unsigned int *generator) {
    assert(write_search_generators || write_all_generators);
    int length = (write_search_generators ? permutation_length : graph_size);
    unordered_map<int, int> gen_map;
    for (int from = 0; from < length; ++from) {
        int to = generator[from];
        if (from != to) {
            gen_map[from] = to;
        }
    }
    to_be_written_generators.push_back(move(gen_map));
}

void Group::add_raw_generator(const unsigned int *generator) {
    Permutation permutation(*this, generator);
    if (permutation.identity()) {
        ++num_identity_generators;
        if (write_all_generators) {
            add_to_be_written_generator(generator);
        }
    } else {
        if (write_search_generators || write_all_generators) {
            add_to_be_written_generator(generator);
        }
        generators.push_back(move(permutation));
    }
}

int Group::get_num_generators() const {
    return generators.size();
}

void Group::dump_generators() const {
    if (get_num_generators() == 0)
        return;

    for (int i = 0; i < get_num_generators(); i++) {
        get_permutation(i).print_affected_variables_by_cycles();
    }

    for (int i = 0; i < get_num_generators(); i++) {
        utils::g_log << "Generator " << i << endl;
        get_permutation(i).print_cycle_notation();
        get_permutation(i).dump_var_vals();
    }

    int num_vars = tasks::g_root_task->get_num_variables();
    utils::g_log << "Extra group info:" << endl;
    utils::g_log << "Number of identity on states generators: " << num_identity_generators << endl;
    utils::g_log << "Permutation length: " << get_permutation_length() << endl;
    utils::g_log << "Permutation variables by values (" << num_vars << "): " << endl;
    for (int i = num_vars; i < get_permutation_length(); i++)
        utils::g_log << get_var_by_index(i) << "  " ;
    utils::g_log << endl;
}

void Group::dump_variables_equivalence_classes() const {
    if (get_num_generators() == 0)
        return;

    int num_vars = tasks::g_root_task->get_num_variables();

    vector<int> vars_mapping;
    for (int i=0; i < num_vars; ++i)
        vars_mapping.push_back(i);

    bool change = true;
    while (change) {
        change = false;
        for (int i = 0; i < get_num_generators(); i++) {
            const std::vector<int>& affected = get_permutation(i).get_affected_vars();
            int min_ind = num_vars;
            for (int var : affected) {
                if (min_ind > vars_mapping[var])
                    min_ind = vars_mapping[var];
            }
            for (int var : affected) {
                if (vars_mapping[var] > min_ind)
                    change = true;
                vars_mapping[var] = min_ind;
            }
        }
    }
    utils::g_log << "Equivalence relation:" << endl;
    for (int i=0; i < num_vars; ++i) {
        vector<int> eqiv_class;
        for (int j=0; j < num_vars; ++j)
            if (vars_mapping[j] == i)
                eqiv_class.push_back(j);
        if (eqiv_class.size() <= 1)
            continue;
        utils::g_log << "[";
        for (int var : eqiv_class)
            utils::g_log << " " << tasks::g_root_task->get_fact_name(FactPair(var, 0));
        utils::g_log << " ]" << endl;
    }
}

void Group::statistics() const {
    utils::g_log << "Size of the grounded symmetry graph: "
         << graph_size << endl;
    utils::g_log << "Number of search generators (affecting facts): "
         << get_num_generators() << endl;
    utils::g_log << "Number of identity generators (on facts, not on operators): "
         << get_num_identity_generators() << endl;
    utils::g_log << "Total number of generators: "
         << get_num_generators() + get_num_identity_generators() << endl;

    if (dump_permutations) {
        dump_generators();
        dump_variables_equivalence_classes();
    }
}

// ===============================================================================
// Methods related to symmetric lookups

void Group::compute_random_symmetric_state(const State &state,
                                           StateRegistry &symmetric_states_registry,
                                           std::vector<State> &states) const {
    // The state must be unpacked. When called from symmetric lookups heuristic,
    // the state is unregistered and therefore unpacked.
    State state_copy = symmetric_states_registry.register_state_buffer(state.get_unpacked_values());
    // Perform random walk of length random_walk_length to compute a single random symmetric state.
    State current_state = state_copy;
    current_state.unpack();
    for (int i = 0; i < rw_length_or_number_symmetric_states; ++i) {
        int gen_no = rng->random(get_num_generators());
        current_state = symmetric_states_registry.permute_state(current_state, get_permutation(gen_no));
        current_state.unpack();
    }
    // Only take the resulting state if it differs from the given one
    if (current_state.get_id() != state_copy.get_id()) {
        states.push_back(current_state);
    }
}

void Group::compute_subset_all_symmetric_states(const State &state,
                                                StateRegistry &symmetric_states_registry,
                                                vector<State> &states) const {
    // TODO: improve efficiency by disallowing a permutation to be applied
    // sequently more than its order times?
//    utils::g_log << "original  state: ";
//    for (size_t i = 0; i < g_variable_domain.size(); ++i) {
//        utils::g_log << i << "=" << state[i] << ",";
//    }
//    utils::g_log << endl;

    /*
      Systematically generate symmetric states until the wished number is
      reached (rw_length_or_number_symmetric_states) or until all states
      have been generated.
    */
    int num_vars = tasks::g_root_task->get_num_variables();
    // The state must be unpacked. When called from symmetric lookups heuristic,
    // the state is unregistered and therefore unpacked.
    State state_copy = symmetric_states_registry.register_state_buffer(state.get_unpacked_values());
    queue<StateID> open_list;
    open_list.push(state_copy.get_id());
    int num_gen = get_num_generators();
    PerStateInformation<bool> reached(false);
    reached[state_copy] = true;
    while (!open_list.empty()) {
        State current_state = symmetric_states_registry.lookup_state(open_list.front());
        current_state.unpack();
        open_list.pop();
        for (int i = 0; i < num_vars; ++i) {
            assert(current_state[i].get_value() >= 0
                && current_state[i].get_value() < tasks::g_root_task->get_variable_domain_size(i));
        }
        for (int gen_no = 0; gen_no < num_gen; ++gen_no) {
            State permuted_state =
                symmetric_states_registry.permute_state(current_state, get_permutation(gen_no));
            for (int i = 0; i < num_vars; ++i) {
                assert(permuted_state[i].get_value() >= 0
                    && permuted_state[i].get_value() < tasks::g_root_task->get_variable_domain_size(i));
            }
//            utils::g_log << "applying generator " << gen_no << " to state " << current_state.get_id() << endl;
//            utils::g_log << "symmetric state: ";
//            for (size_t i = 0; i < g_variable_domain.size(); ++i) {
//                utils::g_log << i << "=" << permuted_state[i] << ",";
//            }
//            utils::g_log << endl;
            if (!reached[permuted_state]) {
//                utils::g_log << "its a new state, with id " << permuted_state.get_id() << endl;
                states.push_back(permuted_state);
                if (static_cast<int>(states.size()) == rw_length_or_number_symmetric_states) {
                    // If number_of_states == -1, this test can never trigger and hence
                    // we compute the set of all symmetric states as desired.
                    return;
                }
                reached[permuted_state] = true;
                open_list.push(permuted_state.get_id());
            }
        }
    }
}

void Group::compute_symmetric_states(const State &state,
                                     StateRegistry &symmetric_states_registry,
                                     vector<State> &states) const {
    if (symmetrical_lookups == SymmetricalLookups::ONE_STATE) {
        compute_random_symmetric_state(state,
                                       symmetric_states_registry,
                                       states);
    } else if (symmetrical_lookups == SymmetricalLookups::SUBSET_OF_STATES ||
               symmetrical_lookups == SymmetricalLookups::ALL_STATES) {
        compute_subset_all_symmetric_states(state,
                                            symmetric_states_registry,
                                            states);
    }
}

vector<int> Group::get_canonical_representative(const State &state) const {
    assert(has_symmetries());
    state.unpack();
    vector<int> canonical_state = state.get_unpacked_values();

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i=0; i < get_num_generators(); i++) {
            if (generators[i].replace_if_less(canonical_state)) {
                changed =  true;
            }
        }
    }
    return canonical_state;
}

vector<int> Group::compute_permutation_trace_to_canonical_representative(const State &state) const {
    // TODO: duplicate code with get_canonical_representative
    assert(has_symmetries());
    state.unpack();
    vector<int> canonical_state = state.get_unpacked_values();

    vector<int> permutation_trace;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i=0; i < get_num_generators(); i++) {
            if (generators[i].replace_if_less(canonical_state)) {
                permutation_trace.push_back(i);
                changed = true;
            }
        }
    }
    return permutation_trace;
}

RawPermutation Group::compute_permutation_from_trace(const vector<int> &permutation_trace) const {
    assert(has_symmetries());
    RawPermutation new_perm = new_identity_raw_permutation();
    for (int permutation_index : permutation_trace) {
        const Permutation &permutation = generators[permutation_index];
        RawPermutation temp_perm(permutation_length);
        for (int i = 0; i < permutation_length; i++) {
           temp_perm[i] = permutation.get_value(new_perm[i]);
        }
        new_perm.swap(temp_perm);
    }
    return new_perm;
}

RawPermutation Group::compute_inverse_permutation(const RawPermutation &permutation) const {
    RawPermutation result(permutation_length);
    for (int i = 0; i < permutation_length; ++i) {
        result[permutation[i]] = i;
    }
    return result;
}

RawPermutation Group::new_identity_raw_permutation() const {
    RawPermutation result(permutation_length);
    iota(result.begin(), result.end(), 0);
    return result;
}

RawPermutation Group::compose_permutations(
    const RawPermutation &permutation1, const RawPermutation & permutation2) const {
    RawPermutation result(permutation_length);
    for (int i = 0; i < permutation_length; i++) {
       result[i] = permutation2[permutation1[i]];
    }
    return result;
}

RawPermutation Group::create_permutation_from_state_to_state(
        const State& from_state, const State& to_state) const {
    assert(has_symmetries());
    vector<int> from_state_permutation_trace = compute_permutation_trace_to_canonical_representative(from_state);
    vector<int> to_state_permutation_trace = compute_permutation_trace_to_canonical_representative(to_state);

    RawPermutation canonical_to_to_state_permutation = compute_inverse_permutation(compute_permutation_from_trace(to_state_permutation_trace));
    RawPermutation from_state_to_canonical_permutation = compute_permutation_from_trace(from_state_permutation_trace);
    return compose_permutations(from_state_to_canonical_permutation, canonical_to_to_state_permutation);
}

int Group::get_var_by_index(int ind) const {
    // In case of ind < num_vars, returns the index itself, as this is the variable part of the permutation.
    if (ind < num_vars) {
        utils::g_log << "=====> WARNING!!!! Check that this is done on purpose!" << endl;
        return ind;
    }
    return var_by_val[ind-num_vars];
}

std::pair<int, int> Group::get_var_val_by_index(const int ind) const {
    assert(ind>=num_vars);
    int var =  var_by_val[ind-num_vars];
    int val = ind - dom_sum_by_var[var];

    return make_pair(var, val);
}

int Group::get_index_by_var_val_pair(const int var, const int val) const {
    return dom_sum_by_var[var] + val;
}

static class GroupCategoryPlugin : public plugins::TypedCategoryPlugin<Group> {
public:
    GroupCategoryPlugin() : TypedCategoryPlugin("Group") {
        // TODO: Replace add synopsis for the wiki page.
        // document_synopsis("...");
        allow_variable_binding();
    }
}
_category_plugin;

class GroupFeature : public plugins::TypedFeature<Group, Group> {
public:
    GroupFeature() : TypedFeature("structural_symmetries") {
        document_title("Group");
        document_synopsis("");
        // General Bliss options and options for GraphCreator
        add_option<int>("time_bound",
                               "Stopping after the Bliss software reached the time bound",
                               "0");
        add_option<bool>("stabilize_initial_state",
                                "Compute symmetries stabilizing the initial state",
                                "false");
        add_option<bool>("stabilize_goal",
                                "Compute symmetries stabilizing the goal",
                                "true");
        add_option<bool>("use_color_for_stabilizing_goal",
                                "Use a color to stabilize the goal instead of "
                                "using an additional node linked to goal values.",
                                "true");
        add_option<bool>("dump_symmetry_graph",
                                "Dump symmetry graph in dot format",
                                "false");

        // Type of search symmetries to be used
        add_option<SearchSymmetries>("search_symmetries",
                                                 "Choose the type of structural symmetries that "
                                                 "should be used for pruning: OSS for orbit space "
                                                 "search or DKS for storing the canonical "
                                                 "representative of every state during search",
                                                 "NONE");

        // Options for symmetric lookup symmetries
        add_option<SymmetricalLookups>("symmetrical_lookups",
                                       "Choose the options for using symmetric lookups, "
                                       "i.e. what symmetric states should be computed "
                                       "for every heuristic evaluation:\n"
                                       "- ONE_STATE: one random state, generated through a random "
                                       "walk of length specified via length_or_number option\n"
                                       "- SUBSET_OF_STATES: a subset of all symmetric states, "
                                       "generated in the same systematic way as generating "
                                       "all symmetric states, of size specified via "
                                       "length_or_Number option\n"
                                       "- ALL_STATES: all symmetric states, generated via BFS in "
                                       "the orbit.",
                                       "NONE");
        add_option<int>("symmetry_rw_length_or_number_states",
                        "Choose the length of a random walk if sl_type="
                        "ONE or the number of symmetric states if sl_type="
                        "SUBSET",
                        "5");

        utils::add_rng_options(*this);

        add_option<bool>("dump_permutations",
                                "Dump the generators",
                                "false");
        add_option<bool>(
            "write_search_generators",
            "Write symmetry group generators that affect variables to a file and "
            "stop afterwards.",
            "false");
        add_option<bool>(
            "write_all_generators",
            "Write all symmetry group generators to a file, including those that "
            "do not affect variables, and stop afterwards.",
            "false");
    }
};

static plugins::FeaturePlugin<GroupFeature> _plugin;

static plugins::TypedEnumPlugin<SearchSymmetries> _enum_search_symmetries_plugin({
    {"NONE", ""},
    {"OSS", ""},
    {"DKS", ""},
});

static plugins::TypedEnumPlugin<SymmetricalLookups> _enum_symmetrical_lookups_plugin({
    {"NONE", ""},
    {"ONE_STATE", ""},
    {"SUBSET_OF_STATES", ""},
    {"ALL_STATES", ""},
});
