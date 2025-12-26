#!/usr/bin/env python3
import requests
import tarfile
import os

# Zenodo download URL
URL = "https://zenodo.org/records/17905965/files/cpu2017.trace.tar.gz"
OUTFILE = "cpu2017.trace.tar.gz"
CHUNK_SIZE = 1024 * 1024  # 1MB


def download(url, out):
    print(f"[+] Downloading from:\n    {url}")
    print(f"[+] Saving as: {out}")

    headers = {}
    downloaded_bytes = 0

    if os.path.exists(out):
        downloaded_bytes = os.path.getsize(out)
        headers["Range"] = f"bytes={downloaded_bytes}-"
        print(f"[+] Resuming download from byte {downloaded_bytes}")

    with requests.get(url, headers=headers, stream=True, timeout=60) as r:
        r.raise_for_status()

        mode = "ab" if downloaded_bytes > 0 else "wb"
        total_size = r.headers.get("Content-Length")
        if total_size is not None:
            total_size = int(total_size) + downloaded_bytes

        with open(out, mode) as f:
            for chunk in r.iter_content(chunk_size=CHUNK_SIZE):
                if chunk:
                    f.write(chunk)

    print("[+] Download complete.")


def extract_tar_gz(filename):
    print(f"[+] Extracting {filename} ...")

    if not tarfile.is_tarfile(filename):
        raise ValueError("File is not a valid tar.gz archive.")

    with tarfile.open(filename, "r:gz") as tar:
        tar.extractall(".")

    print("[+] Extraction complete. Files extracted to ./")


def main():
    # Step 1: download file (resume-safe)
    download(URL, OUTFILE)

    # Step 2: extract tar.gz
    extract_tar_gz(OUTFILE)

    print("\n[✓] All steps completed successfully.")


if __name__ == "__main__":
    main()
