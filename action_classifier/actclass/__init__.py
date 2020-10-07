import datetime
import os
import time
from typing import List

import actclass.dataset
import actclass.exec
import actclass.plot
import actclass.util


objects = ['bowl', 'knife', 'screwdriver', 'cuttingboard', 'whisk', 'hammer', 'bottle', 'cup',
           'banana', 'cereals', 'sponge', 'woodenwedge', 'saw', 'harddrive', 'left_hand',
           'right_hand']
relations = ['contact', 'above', 'below', 'left of', 'right of', 'behind of', 'in front of',
             'inside', 'surround', 'moving together', 'halting together', 'fixed moving together',
             'getting close', 'moving apart', 'stable', 'temporal']
actions = ['idle', 'approach', 'retreat', 'lift', 'place', 'hold', 'pour', 'cut', 'hammer', 'saw',
           'stir', 'screw', 'drink', 'wipe']

verbosity: int = 0
write_logfile: bool = False
logfile_path: str = ''
logfile_name: str = ''
logfile_entries: List[str] = []
last_log_time: float = time.time()
log_interval: float = 0.1  # Throttle logfile updates to once each X seconds.


def wait_for_logfile():
    if not write_logfile:
        return
    wait_maximum: float = 5  # Maximum amount of frames to wait for logfile to be written.
    start_time = time.time()
    written = False
    while not written:
        logfile = os.path.join(logfile_path, logfile_name)
        try:
            with open(logfile, 'w') as f:
                for line in logfile_entries:
                    f.write('{}\n'.format(line))
        except:
            time.sleep(0.25)
            if time.time() - start_time > wait_maximum:
                print('Giving up writing logfile.')
                written = True
        else:
            written = True


def print_generic(text: str, level: int, silent: bool = False) -> None:
    global last_log_time, logfile_path, logfile_name
    dt_object = datetime.datetime.fromtimestamp(time.time())
    dt_string = '{}'.format(dt_object)
    line = '[{}][v{}] {}'.format(dt_string, level, text)
    logfile_entries.append(line)
    if verbosity >= level:
        print(line)

    if write_logfile and not silent:
        current_time = time.time()
        if current_time - last_log_time > log_interval:
            if logfile_name == '':
                dt_string_underscore = dt_string.replace(' ', '_').replace('-', '_').replace('.', '_').replace(':', '_')
                logfile_name = 'run_{}.log'.format(dt_string_underscore)
            logfile = os.path.join(logfile_path, logfile_name)
            try:
                with open(logfile, 'w') as f:
                    for line in logfile_entries:
                        f.write('{}\n'.format(line))
            except IOError:
                print0('Could not write logfile.', silent=True)
                pass
            last_log_time = current_time


def print0(text: str, silent=False) -> None:
    print_generic(text, 0, silent=silent)


def print1(text: str) -> None:
    print_generic(text, 1)


def print2(text: str) -> None:
    print_generic(text, 2)


def print3(text: str) -> None:
    print_generic(text, 3)


def print4(text: str) -> None:
    print_generic(text, 4)
