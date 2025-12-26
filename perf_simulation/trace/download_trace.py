#!/usr/bin/env python3
import subprocess
import sys

URL = "https://zenodo.org/records/17905965/files/cpu2017.trace.tar.gz"

def main():
    cmd = ["wget", "-c", URL]
    print("[+] Running:", " ".join(cmd))

    ret = subprocess.call(cmd)
    if ret != 0:
        print("[!] wget failed")
        sys.exit(1)

    print("[✓] Download completed successfully.")

if __name__ == "__main__":
    main()
