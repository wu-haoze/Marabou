#!/usr/bin/env python3

"""gg-Marabou test runner

Usage:
  runner.py run [options] <net> <prop>
  runner.py list
  runner.py (-h | --help)

Options:
  --jobs N              The number of jobs [default: 1]
  --initial-divides N   The initial number of divides [default: 0]
  --online-divides N    The number of divides to do on timeout [default: 2]
  --divide-strategy S   The divide strategy [default: largest-interval]
  --timeout N           How long to try for (s) [default: 3600]
  --initial-timeout N   How long to try for (s) before splitting [default: 5]
  --timeout-factor N    How long to multiply the initial_timeout by each split [default: 1.5]
  --infra I             gg-local, gg-lambda, or thread [default: gg-local]
  --trial N             the trial number to run [default: 0]
  --lambda              Run the lambda benchmarks
  --local               Run the local benchmarks
  --specific            Run just one benchmark
  --acas                Use ACAS shorthand for network & property files.
  --mnist               Use MNIST shorthand for network & property files.
  -h --help             Show this screen.
"""
from copy import deepcopy
from docopt import docopt
from enum import Enum
from glob import glob
from os import path
from pathlib import Path
from re import search
import os
import pickle
import subprocess as sub
import time

DATA = 'data'
OUTPUT_FILE = 'out.pickle'
SUMMARY_FILE = 'out.txt'
GG_OUTPUT_FILE = 'out'

class Infra(Enum):
    THREAD = 1
    GG_LOCAL = 2
    GG_LAMBDA = 3

INFRA_STRINGS = {
        'thread': Infra.THREAD,
        'gg-local': Infra.GG_LOCAL,
        'gg-lambda': Infra.GG_LAMBDA,
}

def infra_from_string(s):
    assert s in list(INFRA_STRINGS.keys()) + [None], "Invalid --infra"
    return INFRA_STRINGS[s] if s in INFRA_STRINGS else None

LARGEST_INTERVAL =  'largest-interval'
SPLIT_RELU =  'split-relu'

# When you add a field to RunInputs, add it here, so that you stay backwards compatible
RUN_INPUT_ADDITIONS = [
        'trial',
        'divide_strategy',
]
RUN_INPUT_DEFAULTS = {
        'divide_strategy': LARGEST_INTERVAL,
        'trial': 0,

}
RUN_INPUT_DEFAULTS_LIST = [RUN_INPUT_DEFAULTS[p] for p in RUN_INPUT_ADDITIONS]


class RunInputs(object):
    def __init__(self, net, prop, infra, jobs, initial_divides, divides, timeout, initial_timeout, timeout_factor, divide_strategy, trial = 0):
        self.marabou = abs_marabou_repo() + '/build/Marabou'
        self.net = net
        self.prop = prop
        self.infra = infra
        self.jobs = jobs
        self.initial_divides = initial_divides
        self.divides = divides
        self.timeout = timeout
        self.initial_timeout = initial_timeout
        self.timeout_factor = timeout_factor
        self.divide_strategy = divide_strategy
        self.trial = trial
        self.hash = self.get_marabou_hash()

    def net_name(self):
        return os.path.split(self.net)[1]

    def prop_name(self):
        return os.path.split(self.prop)[1]

    def net_path(self):
        return self.net

    def prop_path(self):
        return self.prop

    def __str__(self):
        return self.dash_string(0)

    def key_props(self):
        return [ self.net_name(),
            self.prop_name(),
            self.infra,
            self.jobs,
            self.initial_divides,
            self.divides,
            self.timeout,
            self.initial_timeout,
            self.timeout_factor,
            self.trial,
            self.divide_strategy]

    def dash_string(self, missing = 0):
        displayed = self.key_props()
        return '-'.join(str(d).replace('-','') for d in (displayed[:-missing] if missing > 0 else displayed))


    def get_marabou_hash(self):
        cp = sub.run([self.marabou, '--version'], stdout = sub.PIPE, check = True)
        r = search('[0-9a-f]{8}', cp.stdout.decode())
        assert r is not None
        return r.group(0)

    def run(self):
        if self.infra is Infra.THREAD:
            return self.run_as_threads()
        elif self.infra is Infra.GG_LOCAL or self.infra is Infra.GG_LAMBDA:
            return self.run_as_gg()
        else:
            assert False, f'Unsupport infra {self.infra}'

    def run_data_dir(self):
        existing = set([])
        for n_missing_attrs in range(1, len(RUN_INPUT_ADDITIONS) + 1):
            if self.key_props()[-n_missing_attrs] == RUN_INPUT_DEFAULTS_LIST[-n_missing_attrs]:
                path = os.path.join(DATA, self.dash_string(n_missing_attrs))
                if os.path.exists(os.path.join(path, OUTPUT_FILE)):
                    existing.add(path)
            else:
                break
        if len(existing) > 1:
            pstring = ''.join('\n\t' + str(o) for o in existing)
            assert False, f"Multiple paths match {self}:{pstring}"
        elif len(existing) == 1:
            print(existing)
            return list(existing)[0]
        else:
            return os.path.join(DATA, self.dash_string(0))


    def run_as_gg(self):
        this_data_dir = self.run_data_dir()
        print(f'running in {this_data_dir}')
        output_path = os.path.join(this_data_dir, OUTPUT_FILE)
        gg_output_path = os.path.join(this_data_dir, GG_OUTPUT_FILE)
        if not os.path.exists(output_path):
            self.setup_gg()
            start_time = time.time()
            result = Result.TIMEOUT
            args = self.gg_args()
            try:
                #print(' '.join(args))
                print(args)
                sub.run(args, cwd = this_data_dir, timeout = self.timeout, check = True)
                with open(gg_output_path) as f:
                    first = f.read().split()[0]
                    if first == "SAT":
                        result = Result.SAT
                    elif first == "UNSAT":
                        result = Result.UNSAT
                    else:
                        assert False, f'Unexpected result {first}'
            except sub.TimeoutExpired as e:
                pass
            duration = time.time() - start_time
            output = RunOutputs(self, start_time, duration, result)
            with open(output_path, 'wb') as f:
                pickle.dump(output, f)
                return output
        with open(output_path, 'rb') as f:
            return pickle.load(f)

    def gg_args(self):
        if self.infra is Infra.GG_LOCAL:
            return ['gg-force', '--jobs', str(self.jobs), '--engine', 'local', GG_OUTPUT_FILE]
        elif self.infra is Infra.GG_LAMBDA:
            return ['gg-force', '--jobs', str(self.jobs), '--engine', 'lambda',
                                '--jobs', str(1), '--fallback-engine', 'local', GG_OUTPUT_FILE]
        else:
            assert False, f'Unsupport infra {self.infra}'

    def setup_gg(self):
        merge_path = abs_marabou_repo() + "/build/gg/merge"
        create_path = abs_marabou_repo() + "/gg/create-thunks.zsh"
        this_data_dir = self.run_data_dir()
        os.makedirs(this_data_dir, exist_ok = True)
        args = [ create_path,
                 self.marabou,
                 merge_path,
                 self.net_path(),
                 self.prop_path(),
                 str(self.initial_divides),
                 str(self.divides),
                 str(self.initial_timeout),
                 str(self.timeout_factor),
                 self.divide_strategy]
        print(' '.join(args))
        sub.run(args, cwd = this_data_dir, check = True)

    def run_as_threads(self):
        this_data_dir = self.run_data_dir()
        output_path = os.path.join(this_data_dir, OUTPUT_FILE)
        summary_path = os.path.join(this_data_dir, SUMMARY_FILE)
        if not os.path.exists(output_path):
            os.makedirs(this_data_dir, exist_ok = True)
            start_time = time.time()
            result = Result.TIMEOUT
            try:
                args = [ str(s) for s in [
                    self.marabou,
                    '--dnc',
                    '--initial-divides', self.initial_divides,
                    '--initial-timeout', self.initial_timeout,
                    '--timeout-factor', self.timeout_factor,
                    '--num-online-divides', self.divides,
                    '--num-workers', self.jobs,
                    '--verbosity', '0',
                    '--summary-file', SUMMARY_FILE,
                    '--divide-strategy', self.divide_strategy,
                    self.net_path(),
                    self.prop_path(),
                ] ]
                print(' '.join(args))
                sub.run(args, cwd = this_data_dir, timeout = self.timeout, check = True)
                with open(summary_path) as f:
                    first = f.read().split()[0]
                    if first == "SAT":
                        result = Result.SAT
                    elif first == "UNSAT":
                        result = Result.UNSAT
                    else:
                        assert False, f'Unexpected result {first}'
            except sub.TimeoutExpired as e:
                pass
            duration = time.time() - start_time
            output = RunOutputs(self, start_time, duration, result)
            with open(output_path, 'wb') as f:
                pickle.dump(output, f)
                return output
        with open(output_path, 'rb') as f:
            return pickle.load(f)


    def __getattr__(self, attr):
        if attr in RUN_INPUT_DEFAULTS:
            return RUN_INPUT_DEFAULTS[attr]
        else:
            raise AttributeError(f"No attribute `{attr}` in RunInputs or its defaults")

def abs_marabou_repo():
    p = Path.cwd().resolve()
    while p.name != 'Marabou':
        p = p.parent
    return str(p)

class Result(Enum):
    SAT = 1
    UNSAT = 2
    TIMEOUT = 3

# When you add a field to RunOutputs, add it here, so that you stay backwards compatible
RUN_OUTPUT_ADDITIONS = [
        'region',
]
RUN_OUTPUT_DEFAULTS = {
        'region': 'us-west-2',

}

class RunOutputs(object):
    def __init__(self, inputs, start_time, runtime, result):
        self.inputs = inputs
        self.start_time = start_time
        self.runtime = runtime
        self.result = result
        self.region = os.environ['AWS_REGION']

    def csv_header():
        return [
            'net',
            'prop',
            'infra',
            'jobs',
            'runtime',
            'initial_divides',
            'divides',
            'timeout',
            'initial_timeout',
            'timeout_factor',
            'hash',
            'trial',
            'divide_strategy',
            'region',
            'start_time',
            'result',
        ]

    def csv_row(self):
        return [
            str(self.inputs.net_name()),
            str(self.inputs.prop_name()),
            str(self.inputs.infra),
            str(self.inputs.jobs),
            str(self.runtime),
            str(self.inputs.initial_divides),
            str(self.inputs.divides),
            str(self.inputs.timeout),
            str(self.inputs.initial_timeout),
            str(self.inputs.timeout_factor),
            str(self.inputs.hash),
            str(self.inputs.trial),
            str(self.inputs.divide_strategy),
            str(self.region),
            str(self.start_time),
            str(self.result),
        ]

    def __getattr__(self, attr):
        if attr in RUN_OUTPUT_DEFAULTS:
            return RUN_OUTPUT_DEFAULTS[attr]
        else:
            raise AttributeError(f"No attribute `{attr}` in RunOutputs or its defaults")

def with_n_trials(inputs, N):
    def help():
        for i in inputs:
            for t in range(N):
                i1 = deepcopy(i)
                i1.trial = t
                yield i1
    return list(help())

def with_all_infras(inputs):
    def help():
        for i in inputs:
            for infra in list(Infra):
                i1 = deepcopy(i)
                i1.infra = infra
                yield i1
    return list(help())

def with_local_infras(inputs):
    def help():
        for i in inputs:
            for infra in [Infra.GG_LOCAL, Infra.THREAD]:
                i1 = deepcopy(i)
                i1.infra = infra
                yield i1
    return list(help())

def with_all_jobs_counts(inputs, job_counts):
    def help(inputs):
        for i in inputs:
            for j in job_counts:
                i1 = deepcopy(i)
                i1.jobs = j
                i1.initial_divides = log2(j)
                yield i1
    return list(help(inputs))

def log2(i):
    r2 = 1
    r = 0
    while i > r2:
        r2 *= 2
        r += 1
    return r

if __name__ == '__main__':
    arguments = docopt(__doc__)
    if arguments['run']:
        if arguments['--acas']:
            net = arguments['<net>']
            prop = arguments['<prop>']
            net = f'{abs_marabou_repo()}/gg/acas/ACASXU_run2a_{net}_batch_2000.nnet'
            prop = f'{abs_marabou_repo()}/gg/acas-properties/property{prop}.txt'
        elif arguments['--mnist']:
            net_short = arguments['<net>']
            prop = arguments['<prop>']
            net = f'{abs_marabou_repo()}/gg/mnist/mnist{net_short}.nnet'
            props = glob(f'{abs_marabou_repo()}/gg/mnist-properties/net{net_short}_ind{prop}*.txt')
            assert len(props) == 1
            prop = props[0]
        else:
            net = os.path.abspath(arguments['<net>'])
            prop = os.path.abspath(arguments['<prop>'])
        jobs = int(arguments['--jobs'])
        initial_divides = int(arguments['--initial-divides'])
        online_divides = int(arguments['--online-divides'])
        timeout = int(arguments['--timeout'])
        initial_timeout = int(arguments['--initial-timeout'])
        timeout_factor = float(arguments['--timeout-factor'])
        divide_strategy = arguments['--divide-strategy']
        assert divide_strategy in [SPLIT_RELU, LARGEST_INTERVAL]
        infra = infra_from_string(arguments['--infra'])
        trial = int(arguments['--trial'])
        i = RunInputs(
                net,
                prop,
                infra,
                jobs,
                initial_divides,
                online_divides,
                timeout,
                initial_timeout,
                timeout_factor,
                divide_strategy,
                trial)
        print(i)
        I = []
        if arguments['--specific']:
            I += [i]
        elif arguments['--lambda']:
            I += with_n_trials(with_all_jobs_counts([i], list(reversed([4, 8, 16, 32, 64, 128, 256, 512, 1000]))), 3)
        elif arguments['--local']:
            I += with_n_trials(with_all_jobs_counts(with_local_infras([i]), list(reversed([4, 8, 16]))), 3)
        else:
            assert False

        R = [i.run() for i in I]
        print(RunOutputs.csv_header())
        for r in R:
            print(r.csv_row())
    elif arguments['list']:
        print(','.join(RunOutputs.csv_header()))
        for d in os.listdir(DATA):
            report_path = f'{DATA}/{d}/{OUTPUT_FILE}'
            if os.path.exists(report_path):
                try:
                    with open(report_path, 'rb') as f:
                        o = pickle.load(f)
                        print(','.join(o.csv_row()))
                except:
                    pass
