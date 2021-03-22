#! /usr/bin/env python

from lab.parser import Parser

parser = Parser()
parser.add_pattern('symmetrical_states', 'Symmetrical states generated: (\d+)', required=False, type=int)
parser.add_pattern('improved_evaluations', 'Symmetry-improved evaluations: (\d+)', required=False, type=int)
parser.add_pattern('improving_states', 'Improving symmetrical states: (\d+)', required=False, type=int)
parser.add_pattern('last_jump_symmetrical_states', 'Symmetrical states generated until last jump: (\d+)', required=False, type=int)
parser.add_pattern('last_jump_improved_evaluations', 'Symmetry-improved evaluations until last jump: (\d+)', required=False, type=int)
parser.add_pattern('last_jump_improving_states', 'Improving symmetrical states until last jump: (\d+)', required=False, type=int)
parser.add_pattern('num_search_generators', 'Number of search generators \(affecting facts\): (\d+)', required=False, type=int)
parser.add_pattern('num_operator_generators', 'Number of identity generators \(on facts, not on operators\): (\d+)', required=False, type=int)
parser.add_pattern('num_total_generators', 'Total number of generators: (\d+)', required=False, type=int)
parser.add_pattern('symmetry_graph_size', 'Size of the grounded symmetry graph: (\d+)', required=False, type=int)
parser.add_pattern('time_symmetries', 'Done initializing symmetries: (.+)s', required=False, type=float)
parser.add_pattern('symmetry_group_order', 'Symmetry group order: (\d+)', required=False, type=int)

def compute_average(content, props):
    symmetrical_states_per_evaluation = 0
    symmetrical_states = props.get('symmetrical_states', None)
    if symmetrical_states:
        evaluations = props.get('evaluations', None)
        if evaluations:
            symmetrical_states_per_evaluation = symmetrical_states / evaluations
    props['symmetrical_states_per_evaluation'] = symmetrical_states_per_evaluation

    last_jump_symmetrical_states_per_evaluation = 0
    last_jump_symmetrical_states = props.get('last_jump_symmetrical_states', None)
    if last_jump_symmetrical_states:
        evaluations_until_last_jump = props.get('evaluations_until_last_jump', None)
        if evaluations_until_last_jump:
            last_jump_symmetrical_states_per_evaluation = last_jump_symmetrical_states / evaluations_until_last_jump
    props['last_jump_symmetrical_states_per_evaluation'] = last_jump_symmetrical_states_per_evaluation

parser.add_function(compute_average)

parser.parse()
