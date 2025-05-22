#!/usr/bin/env python3

import sys, re, os, time
from optparse import OptionParser
from subprocess import Popen

#def send(msg, chat_id, token):
#    bot = telegram.Bot(token=token)
#    bot.sendMessage(chat_id=chat_id, text=msg)

def getcmd(target_cmd_lines, target_script):
    try:
        script = open(target_script, 'r')
    except IOError:
        print("cannot open " + options.simscript)
        sys.exit()

    for line in script:
        line = re.sub('#.*', '', line)
        if len(line) > 2:
            line=line[:-1]
        target_cmd_lines.append(line)

def parse():
    # 1) Option Parse #
    parser = OptionParser()
    parser.add_option("-p", "--process", dest="num_work_process", type="int",
                      help="maximum number of active worker process, default = 4",
                      default=4)
    parser.add_option("-s", "--script", dest="target_script", type="string",
                      help="name of the target shell script",
                      default="/dev/null")
    return parser.parse_args()
  
def main():
    options, _ = parse()
    active_processes=[]
    target_cmd_lines=[]
    getcmd(target_cmd_lines, options.target_script)
    while True:
        try:
            for process in active_processes[:]:
                if process.poll() != None:
                    active_processes.remove(process)
                    print("Process [pid = " + str(process.pid) + "] finished")
            while len(active_processes) < options.num_work_process:
                if len(target_cmd_lines) != 0:
                    cmd = target_cmd_lines.pop()
                else:
                    break
                active_processes.append(Popen([cmd], shell=True))
                print("Process [pid = "+str(active_processes[-1].pid)+"] started -- " + cmd)
            time.sleep(1)
            if len(active_processes) == 0:
                break
        except KeyboardInterrupt:
            print("Keyboard Interrupt!!!!")
            os.killpg(os.getpgrp(), 9)
            for process in active_processes:
                os.kill(process.pid, 9)
            sys.exit()
    
if __name__ == "__main__":
    main()