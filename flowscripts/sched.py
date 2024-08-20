#!/usr/bin/env python3
"""schedule design runs and save in run scripts"""

import os
import sys
import subprocess
import time
from datetime import datetime
import re
from collections import defaultdict

def filelist():
    """return list of relevant filepaths"""
    return subprocess.Popen(
        "/usr/bin/ls */raptor.log */*.script",
        shell=True, stdout=subprocess.PIPE, universal_newlines=True
    ).stdout
# def filelist

def toplogic():
    """do everything"""
    # handle arguments
    args = list(sys.argv)
    # which original device sizes to process?
    if len(args) >= 2 and args[1] == "-s":
        assert len(args) >= 3
        process = set(re.split(r'\s+|,', args[2]))
        del args[1:3]
    else:
        process = set()
    # get mode and slots
    assert len(args) >= 3
    mode = args[1]
    assert re.match(r'[fF]?[cCeE][fF]?$', mode)
    slots = int(args[2])
    assert 0 < slots <= 12
    commercial = bool(re.search(r'[cC]', mode))

    times = defaultdict(int)
    sizes = defaultdict(set)
    debug = False
    blank = ""
    for filename in filelist():
        filename = filename.strip()
        fn = filename.split('/')
        assert len(fn) == 2, f"filename={filename}"
        (design, file) = fn
        match file:
            case "raptor.log":
                with open(filename, encoding='ascii') as ifile:
                    for line in ifile:
                        line = line.strip()
                        # Yosys: End of script.
                        #        Logfile hash: 5a7f8b6754,
                        #        CPU: user 9.96s system 0.38s,
                        #        MEM: 601.86 MB peak
                        if line.startswith("End of script. Logfile hash: "):
                            ll = line.split()
                            d = float(ll[8][:-1]) + float(ll[10][:-2])
                            if debug or d > 7200: print(blank := f"# d={d} line={line}")
                            times[design] += d
                        # VPR: The entire flow of VPR took 124.74 seconds (max_rss 694.1 MiB)
                        if line.startswith("The entire flow of VPR took "):
                            d = float(line.split()[6]) * (2 if commercial else 1)
                            if debug or d > 7200: print(blank := f"# d={d} line={line}")
                            times[design] += d
                        # OpenFPGA: The entire OpenFPGA flow took 186.199 seconds
                        if line.startswith("The entire OpenFPGA flow took "):
                            d = float(line.split()[5])
                            if debug or d > 7200: print(blank := f"# d={d} line={line}")
                            times[design] += d
                        ## CS stuff: Total RunTime: 5754 sec
                        #if line.startswith("Total RunTime"):
                        #    d = float(line.split()[2])
                        #    if debug or d > 7200: print(blank := f"# d={d} line={line}")
                        #    times[design] += d
                        g = re.search(r'\s--device\s+\w+?(\d+x\d+)\w*\s', line)
                        if g:
                            sz = g.group(1)
                            sizes[design].add(sz)
                            #print(f"Adding {sz} to {design}")
                    # for
                # with
            case "bitgen.script" | "lvsbit.script" | "route2repack.script":
                if not commercial and file != "bitgen.script": continue
                # saved script files for these scripts
                # Script started on 2024-07-31 20:47:49-07:00 [TERM=...
                # Script done on 2024-07-31 20:47:49-07:00 [COMMAND_EXIT_CODE="0"]

                saw_start = False
                # stop time defaults to .script modification time
                mtime = os.path.getmtime(filename)
                mtimestr = time.ctime(mtime)
                # 'Wed Aug 14 00:37:46 2024'
                sstop = datetime.strptime(mtimestr, '%a %b %d %H:%M:%S %Y')
                #print(f"filename={filename} sstop={sstop}")

                with open(filename, encoding='ascii') as ifile:
                    for line in ifile:
                        line = line.strip()
                        if  not (started := line.startswith("Script started on ")) and \
                            not line.startswith("Script done on "): continue
                        line = re.sub(r'^.*\s+on\s+', r'', line)
                        line = re.sub(r'\s+\[.*$', r'', line)
                        if line[0].isdigit():
                            # 2024-07-31 20:47:49-07:00
                            d = datetime.strptime(line[:19], '%Y-%m-%d %H:%M:%S')
                        else:
                            # Mon 12 Aug 2024 02:45:32 PM PDT
                            d = datetime.strptime(line, '%a %d %b %Y %I:%M:%S %p %Z')
                        #print(f"filename={filename} d={d} line={line}")
                        if started:
                            saw_start = True
                            sstart = d
                            if debug: print(blank := f"# sstart={sstart} line={line}")
                        else:
                            sstop = d
                            if debug: print(blank := f"# sstop={sstop} line={line}")
                    # for
                # with
                assert saw_start
                d = (sstop - sstart).total_seconds()
                if debug or d > 7200: print(blank := f"# d={d} line={line}")
                times[design] += d
            case _:
                sys.stderr.write(f"Unrecognized file: {file}\n")
                sys.exit(1)
        # match
    # for
    if blank: print("")
    for design, szs in sizes.items():
        assert len(szs) == 1

    jobs = []
    for design, secs in times.items():
        if not sizes[design]:
            print(f"Design {design} had incomplete raptor.log")
            continue
        assert design in sizes, f"design={design}"
        if process and list(sizes[design])[0] not in process: continue
        jobs.append((design, secs))
    jobs = sorted(jobs, key=lambda j: -j[1])

    for design, secs in jobs:
        print(f"# {int(secs):5d} {design}")

    machines = [ [] for _ in range(slots) ]
    alltime = [ 0 for _ in range(slots) ]

    while jobs:
        (d, runtime) = jobs.pop(0)
        slot = sorted(range(slots), key=lambda i: alltime[i])[0]
        machines[slot].append(f"{d}/bit_sim.sh")
        alltime[slot] += runtime
    # while
    peak = max(alltime)

    for s in range(slots):
        t = len(machines[s])
        comment = f"# {s:2d}q {t:2d}# {int(alltime[s])}s {100 * alltime[s] / peak:5.1f}%"
        machs = ' '.join(machines[s])
        cmd = f'script -e -f -q -c "~/bin/raptor_pin_vgl.sh {mode} {machs}" {s}.log'
        print(f'\n{comment}\n{cmd}')
        with open(f"{s}.sh", "w", encoding='ascii') as ofile:
            ofile.write(f'{comment}\n\n{cmd}\n')
    # for
# def

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
