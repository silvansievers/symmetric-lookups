#ifndef SYMMETRICAL_LOOKUPS_HEURISTIC_H
#define SYMMETRICAL_LOOKUPS_HEURISTIC_H

#include "heuristic.h"

class Group;

class SymmetricalLookupsHeuristic : public Heuristic {
    std::shared_ptr<Heuristic> component_heuristic;
    std::shared_ptr<Group> group;
protected:
    virtual int compute_heuristic(const State &ancestor_state) override;
public:
    explicit SymmetricalLookupsHeuristic(const plugins::Options &opts);
    virtual ~SymmetricalLookupsHeuristic() override = default;
};

#endif
