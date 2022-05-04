#! /usr/bin/env python3

import itertools
import os
from subprocess import call

from lab.environments import LocalEnvironment, BaselSlurmEnvironment
from lab.reports import Attribute, arithmetic_mean, geometric_mean

from downward.reports.compare import ComparativeReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ['DOWNWARD_BENCHMARKS']
OLD_REVISION='4ecdf3eb2bde2bdcd5713be953b8fe8719f83919'
REVISION='2f82df4b86e4c7cae9a49ec2cfa2c1172f87e846'
REVISIONS = [OLD_REVISION, REVISION]
CONFIGS = [
    # sl
    IssueConfig('ms-lookups-one5', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=one_state,symmetry_rw_length_or_number_states=5)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym))']),
    IssueConfig('ms-lookups-sub10', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=subset_of_states,symmetry_rw_length_or_number_states=10)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym))']),
    IssueConfig('ms-lookups-all', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=all_states)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym))']),
    IssueConfig('ipdb-lookups-one5', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=one_state, symmetry_rw_length_or_number_states=5)', '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym))']),
    IssueConfig('ipdb-lookups-sub10', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=subset_of_states, symmetry_rw_length_or_number_states=10)', '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym))']),
    IssueConfig('ipdb-lookups-all', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=all_states)', '--heuristic', 'hipdb=ipdb()', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym))']),

    # sl + oss/dks
    IssueConfig('ms-oss-noprune-lookups-one5', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=one_state,symmetry_rw_length_or_number_states=5,search_symmetries=oss)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym),symmetries=sym)']),
    IssueConfig('ms-oss-noprune-lookups-sub10', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=subset_of_states,symmetry_rw_length_or_number_states=10,search_symmetries=oss)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym),symmetries=sym)']),
    IssueConfig('ms-oss-noprune-lookups-all', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=all_states,search_symmetries=oss)', '--heuristic', 'hms=merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,prune_unreachable_states=false)', '--search', 'astar(sl_heuristic(component_heuristic=hms,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-oss-lookups-one5', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=one_state,symmetry_rw_length_or_number_states=5,search_symmetries=oss)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-oss-lookups-sub10', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=subset_of_states,symmetry_rw_length_or_number_states=10,search_symmetries=oss)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-oss-lookups-all', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=all_states,search_symmetries=oss)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-dks-lookups-one5', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=one_state,symmetry_rw_length_or_number_states=5,search_symmetries=dks)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-dks-lookups-sub10', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=subset_of_states,symmetry_rw_length_or_number_states=10,search_symmetries=dks)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
    IssueConfig('ipdb-dks-lookups-all', ['--symmetries', 'sym=structural_symmetries(time_bound=0,symmetrical_lookups=all_states,search_symmetries=dks)',  '--heuristic', 'hipdb=ipdb(max_time=900)', '--search', 'astar(sl_heuristic(component_heuristic=hipdb,symmetries=sym),symmetries=sym)']),
]

SUITE = common_setup.DEFAULT_OPTIMAL_SUITE
ENVIRONMENT = BaselSlurmEnvironment(
    email="silvan.sievers@unibas.ch",
    partition="infai_2",
    export=[],
    # paths obtained via:
    # module purge
    # module -q load CMake/3.15.3-GCCcore-8.3.0
    # module -q load GCC/8.3.0
    # echo $PATH
    # echo $LD_LIBRARY_PATH
    setup='export PATH=/scicore/soft/apps/binutils/2.32-GCCcore-8.3.0/bin:/scicore/soft/apps/CMake/3.15.3-GCCcore-8.3.0/bin:/scicore/soft/apps/cURL/7.66.0-GCCcore-8.3.0/bin:/scicore/soft/apps/bzip2/1.0.8-GCCcore-8.3.0/bin:/scicore/soft/apps/ncurses/6.1-GCCcore-8.3.0/bin:/scicore/soft/apps/GCCcore/8.3.0/bin:/infai/sieverss/repos/bin:/infai/sieverss/local:/export/soft/lua_lmod/centos7/lmod/lmod/libexec:/usr/local/bin:/usr/bin:/usr/local/sbin:/usr/sbin:$PATH\nexport LD_LIBRARY_PATH=/scicore/soft/apps/binutils/2.32-GCCcore-8.3.0/lib:/scicore/soft/apps/cURL/7.66.0-GCCcore-8.3.0/lib:/scicore/soft/apps/bzip2/1.0.8-GCCcore-8.3.0/lib:/scicore/soft/apps/zlib/1.2.11-GCCcore-8.3.0/lib:/scicore/soft/apps/ncurses/6.1-GCCcore-8.3.0/lib:/scicore/soft/apps/GCCcore/8.3.0/lib64:/scicore/soft/apps/GCCcore/8.3.0/lib')

if common_setup.is_test_run():
    SUITE = IssueExperiment.DEFAULT_TEST_SUITE
    ENVIRONMENT = LocalEnvironment(processes=4)

exp = IssueExperiment(
    revisions=REVISIONS,
    configs=CONFIGS,
    environment=ENVIRONMENT,
)
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)

exp.add_parser('symmetries-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

extra_attributes=[
    Attribute('num_search_generators', absolute=True, min_wins=False),
    Attribute('num_operator_generators', absolute=True, min_wins=False),
    Attribute('num_total_generators', absolute=True, min_wins=False),
    Attribute('symmetry_graph_size', absolute=True, min_wins=True),
    Attribute('time_symmetries', absolute=False, min_wins=True, function=geometric_mean),
    Attribute('symmetry_group_order', absolute=True, min_wins=False),

    Attribute('symmetrical_states', absolute=False, min_wins=False),
    Attribute('improved_evaluations', absolute=False, min_wins=False),
    Attribute('improving_states', absolute=False, min_wins=False),
    Attribute('symmetrical_states_per_evaluation', absolute=False, min_wins=False),
    Attribute('last_jump_symmetrical_states', absolute=False, min_wins=False),
    Attribute('last_jump_improved_evaluations', absolute=False, min_wins=False),
    Attribute('last_jump_improving_states', absolute=False, min_wins=False),
    Attribute('last_jump_symmetrical_states_per_evaluation', absolute=False, min_wins=False),
]
attributes = list(exp.DEFAULT_TABLE_ATTRIBUTES)
attributes.extend(extra_attributes)

REVISION=REVISIONS[0]
exp.add_absolute_report_step(
    filter_algorithm=[
        '{}-ms-lookups-one5'.format(REVISION),
        '{}-ms-lookups-sub10'.format(REVISION),
        '{}-ms-lookups-all'.format(REVISION),
        '{}-ms-oss-noprune-lookups-one5'.format(REVISION),
        '{}-ms-oss-noprune-lookups-sub10'.format(REVISION),
        '{}-ms-oss-noprune-lookups-all'.format(REVISION),
        '{}-ipdb-lookups-one5'.format(REVISION),
        '{}-ipdb-lookups-sub10'.format(REVISION),
        '{}-ipdb-lookups-all'.format(REVISION),
        '{}-ipdb-dks-lookups-one5'.format(REVISION),
        '{}-ipdb-dks-lookups-sub10'.format(REVISION),
        '{}-ipdb-dks-lookups-all'.format(REVISION),
        '{}-ipdb-oss-lookups-one5'.format(REVISION),
        '{}-ipdb-oss-lookups-sub10'.format(REVISION),
        '{}-ipdb-oss-lookups-all'.format(REVISION),
    ],
    attributes=attributes,
)

exp.add_fetcher(
    'data/2021-03-22-lmcut-oss-dks-eval',
    filter_algorithm=[
        '{}-ms-lookups-one5'.format(OLD_REVISION),
        '{}-ms-lookups-sub10'.format(OLD_REVISION),
        '{}-ms-lookups-all'.format(OLD_REVISION),
        '{}-ms-oss-noprune-lookups-one5'.format(OLD_REVISION),
        '{}-ms-oss-noprune-lookups-sub10'.format(OLD_REVISION),
        '{}-ms-oss-noprune-lookups-all'.format(OLD_REVISION),
        '{}-ipdb-lookups-one5'.format(OLD_REVISION),
        '{}-ipdb-lookups-sub10'.format(OLD_REVISION),
        '{}-ipdb-lookups-all'.format(OLD_REVISION),
        '{}-ipdb-dks-lookups-one5'.format(OLD_REVISION),
        '{}-ipdb-dks-lookups-sub10'.format(OLD_REVISION),
        '{}-ipdb-dks-lookups-all'.format(OLD_REVISION),
        '{}-ipdb-oss-lookups-one5'.format(OLD_REVISION),
        '{}-ipdb-oss-lookups-sub10'.format(OLD_REVISION),
        '{}-ipdb-oss-lookups-all'.format(OLD_REVISION),
    ],
    merge=True
)

outfile = os.path.join(
    exp.eval_dir,
    "%s-%s-%s-compare.%s" % (
        exp.name, OLD_REVISION, REVISION, 'html'))
name='compare-ms'
exp.add_report(
    ComparativeReport(
        algorithm_pairs=[
            ('{}-ms-lookups-one5'.format(OLD_REVISION), '{}-ms-lookups-one5'.format(REVISION)),
            ('{}-ms-lookups-sub10'.format(OLD_REVISION), '{}-ms-lookups-sub10'.format(REVISION)),
            ('{}-ms-lookups-all'.format(OLD_REVISION), '{}-ms-lookups-all'.format(REVISION)),
            ('{}-ms-oss-noprune-lookups-one5'.format(OLD_REVISION), '{}-ms-oss-noprune-lookups-one5'.format(REVISION)),
            ('{}-ms-oss-noprune-lookups-sub10'.format(OLD_REVISION), '{}-ms-oss-noprune-lookups-sub10'.format(REVISION)),
            ('{}-ms-oss-noprune-lookups-all'.format(OLD_REVISION), '{}-ms-oss-noprune-lookups-all'.format(REVISION)),
        ],
        attributes=attributes,
    ),
    outfile=outfile,
    name=name,
)
exp.add_step('publish-%s' % name, call, ['publish', outfile])

outfile = os.path.join(
    exp.eval_dir,
    "%s-%s-%s-compare.%s" % (
        exp.name, OLD_REVISION, REVISION, 'html'))
name='compare-ipdb'
exp.add_report(
    ComparativeReport(
        algorithm_pairs=[
            ('{}-ipdb-lookups-one5'.format(OLD_REVISION), '{}-ipdb-lookups-one5'.format(REVISION)),
            ('{}-ipdb-lookups-sub10'.format(OLD_REVISION), '{}-ipdb-lookups-sub10'.format(REVISION)),
            ('{}-ipdb-lookups-all'.format(OLD_REVISION), '{}-ipdb-lookups-all'.format(REVISION)),
            ('{}-ipdb-dks-lookups-one5'.format(OLD_REVISION), '{}-ipdb-dks-lookups-one5'.format(REVISION)),
            ('{}-ipdb-dks-lookups-sub10'.format(OLD_REVISION), '{}-ipdb-dks-lookups-sub10'.format(REVISION)),
            ('{}-ipdb-dks-lookups-all'.format(OLD_REVISION), '{}-ipdb-dks-lookups-all'.format(REVISION)),
            ('{}-ipdb-oss-lookups-one5'.format(OLD_REVISION), '{}-ipdb-oss-lookups-one5'.format(REVISION)),
            ('{}-ipdb-oss-lookups-sub10'.format(OLD_REVISION), '{}-ipdb-oss-lookups-sub10'.format(REVISION)),
            ('{}-ipdb-oss-lookups-all'.format(OLD_REVISION), '{}-ipdb-oss-lookups-all'.format(REVISION)),
        ],
        attributes=attributes,
    ),
    outfile=outfile,
    name=name,
)
exp.add_step('publish-%s' % name, call, ['publish', outfile])

exp.run_steps()
