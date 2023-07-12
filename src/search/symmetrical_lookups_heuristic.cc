#include "symmetrical_lookups_heuristic.h"

#include "search_statistics.h"
#include "state_registry.h"

#include "plugins/options.h"
#include "plugins/plugin.h"
#include "structural_symmetries/group.h"
#include "tasks/root_task.h"
#include "utils/logging.h"

using namespace std;

SymmetricalLookupsHeuristic::SymmetricalLookupsHeuristic(const plugins::Options &opts)
    : Heuristic(opts),
      component_heuristic(dynamic_pointer_cast<Heuristic>(opts.get<shared_ptr<Evaluator>>("component_heuristic"))),
      group(opts.get<shared_ptr<Group>>("symmetries")) {
    if (!group->is_initialized()) {
        utils::g_log << "Initializing symmetries (symmetrical lookups heuristic)" << endl;
        group->compute_symmetries(task_proxy);
    }
    g_symmetrical_states_generated = 0;
    g_symmetry_improved_evaluations = 0;
    g_improving_symmetrical_states = 0;
}

int SymmetricalLookupsHeuristic::compute_heuristic(const State &ancestor_state) {
    /*
      Note: the resulting state is unregistered. Hence we need to use its raw
      data for the operations in Group.
    */
    State state = convert_ancestor_state(ancestor_state);
    int value = component_heuristic->compute_heuristic(state);
    /*
      If symmetries are present in the domain and the heuristic value is not
      already a dead-end, then we evaluate symmetric states.
    */
    if (group->has_symmetries()
            && group->get_symmetrical_lookups() != SymmetricalLookups::NONE
            && value != DEAD_END) {
        TaskProxy task_proxy(*tasks::g_root_task);
        StateRegistry tmp_registry(task_proxy);
        vector<State> symmetrical_states;
        group->compute_symmetric_states(state,
                                        tmp_registry,
                                        symmetrical_states);

        int previous_heuristic = value; // only for statistics
        for (size_t i = 0; i < symmetrical_states.size(); ++i) {
            const State &symmetrical_state = symmetrical_states[i];
            for (int j = 0; j < tasks::g_root_task->get_num_variables(); ++j) {
                assert(symmetrical_state[j].get_value() >= 0
                    && symmetrical_state[j].get_value() < tasks::g_root_task->get_variable_domain_size(j));
            }
            int symmetric_heuristic =
                component_heuristic->compute_heuristic(symmetrical_state);
            if (symmetric_heuristic == DEAD_END) {
                ++g_improving_symmetrical_states;
                value = DEAD_END;
                break;
            }
            if (symmetric_heuristic > previous_heuristic) {
                ++g_improving_symmetrical_states;
            }
            if (symmetric_heuristic > value) {
                value = symmetric_heuristic;
            }
        }
        g_symmetrical_states_generated += symmetrical_states.size();
        if (value > previous_heuristic || value == DEAD_END) {
            ++g_symmetry_improved_evaluations;
        }
    }
    return value;
}

class SymmetricalLookupsHeuristicFeature : public  plugins::TypedFeature<Evaluator, SymmetricalLookupsHeuristic>{
public:
    SymmetricalLookupsHeuristicFeature() : TypedFeature("sl_heuristic") {
        document_title("Symmetrical Lookups Heuristic");
        document_synopsis("");
        Heuristic::add_options_to_feature(*this);
        add_option<shared_ptr<Evaluator>>(
            "component_heuristic",
            "any heuristic to be used with symmetrical lookups");
        add_option<shared_ptr<Group>>(
            "symmetries",
            "symmetries object to compute bliss symmetries",
            plugins::ArgumentInfo::NO_DEFAULT);
    }

    virtual shared_ptr<SymmetricalLookupsHeuristic> create_component(const plugins::Options &options, const utils::Context &context) const override {
        if (options.contains("symmetries")) {
            shared_ptr<Group> group = options.get<shared_ptr<Group>>("symmetries");
            if (group->get_symmetrical_lookups() == SymmetricalLookups::NONE) {
                context.error("Symmetries option passed to symmetrical lookups heuristic, but no symmetries should be used.");
            }
        }
        return make_shared<SymmetricalLookupsHeuristic>(options);
    }
};

static plugins::FeaturePlugin<SymmetricalLookupsHeuristicFeature> _plugin;
