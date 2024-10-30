#!/usr/bin/env python3
import argparse
import pcapng
import zipfile
import hashlib

def extract_packets(pcap_file):
    """Reads a wireshark packet capture and extracts the binary packets"""
    packets = []
    with open(pcap_file, 'rb') as fp:
        scanner = pcapng.FileScanner(fp)
        for block in scanner:
            if isinstance(block, pcapng.blocks.EnhancedPacket):
                packets.append(block.packet_data)
    return packets

def build_corpus_zip(zip_file_output, packets):
    """Builds a zip file with a file per packet

    The structure of this zip corpus is a simple content addressable storage
    i.e. seed_file_name == sha256_digest(packet).
    """
    with zipfile.ZipFile(zip_file_output, 'a') as out:
        for packet in packets:
            hash = hashlib.sha256(packet).hexdigest()
            if hash not in out.namelist():
                out.writestr(hash, packet)


def main(pcap_file, output_zip_file):
    packets = extract_packets(pcap_file)
    build_corpus_zip(output_zip_file, packets)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog = "pcapng_to_corpus.py",
        description="""Converts a wireshark capture to a zip of binary packet
                    files suitable for an oss-fuzz corpus. In the case the
                    zip corpus already exists, this script will modify
                    the zip file in place adding seed entries.""")
    parser.add_argument('pcapng_capture_file')
    parser.add_argument('oss_fuzz_corpus_zip')
    args = parser.parse_args()
    main(args.pcapng_capture_file, args.oss_fuzz_corpus_zip)
