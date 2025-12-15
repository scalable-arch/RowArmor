#!/usr/bin/env python3
import urllib.request
import tarfile
import os

# Zenodo download URL
URL = "https://zenodo.org/records/17905965/files/cpu2017.trace.tar.gz?download=1"
OUTFILE = "cpu2017.trace.tar.gz"

def download(url, out):
    print(f"[+] Downloading from:\n    {url}")
    print(f"[+] Saving as: {out}")

    urllib.request.urlretrieve(url, out)
    print("[+] Download complete.")

def extract_tar_gz(filename):
    print(f"[+] Extracting {filename} ...")

    if not tarfile.is_tarfile(filename):
        raise ValueError("File is not a valid tar.gz archive.")

    with tarfile.open(filename, "r:gz") as tar:
        tar.extractall(".")
    print("[+] Extraction complete. Files extracted to ./")

def main():
    # Step 1: download file
    if os.path.exists(OUTFILE):
        print(f"[!] {OUTFILE} already exists. Using existing file.")
    else:
        download(URL, OUTFILE)

    # Step 2: extract tar.gz
    extract_tar_gz(OUTFILE)

    print("\n[✓] All steps completed successfully.")

if __name__ == "__main__":
    main()