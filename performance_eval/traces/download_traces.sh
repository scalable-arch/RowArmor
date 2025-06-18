#!/bin/bash

RECORD_ID="15686737"
FILE="cpu2017.trace.tar.gz"
TOFILE="mcsim_cpu2017_traces.tar.gz"
ZENODO_URL="https://zenodo.org/record/${RECORD_ID}/files/${FILE}?download=1"

echo "[+] Downloading $FILE from Zenodo..."
curl -L "$ZENODO_URL" -o "./$FILE"
echo "[+] Download trace files done..."
