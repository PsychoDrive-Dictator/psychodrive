#!/usr/bin/python

import os
import subprocess
import sys
import concurrent.futures
import multiprocessing

scriptPath = os.path.abspath(__file__)
scriptDir = os.path.dirname(scriptPath)
baseDir = os.path.join(scriptDir, "..", "..")
dataDir = os.path.join(baseDir, "data", "chars")

def getVersions(psychodrivePath):
    result = subprocess.run([psychodrivePath, "printversions"], capture_output=True, text=True)
    return [int(v) for v in result.stdout.strip().split('\n') if v]

def discoverCharNames():
    charNames = []
    for name in os.listdir(dataDir):
        charDir = os.path.join(dataDir, name)
        if os.path.isdir(charDir) and name != "common":
            charNames.append(name)
    return sorted(charNames)

def discoverTasks(psychodrivePath):
    versions = getVersions(psychodrivePath)
    charNames = discoverCharNames()

    tasks = []
    for charName in charNames:
        for version in versions:
            tasks.append((charName, version))

    return sorted(tasks)

def cookOne(psychodrivePath, charName, version, outputDir):
    outFile = os.path.join(outputDir, f"{charName}{version}.bin")
    charSpec = charName + str(version)
    result = subprocess.run(
        [psychodrivePath, "cook", charSpec, outFile],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if result.returncode != 0:
        return None
    return outFile

def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <output_dir> [psychodrive_path]")
        sys.exit(1)

    outputDir = sys.argv[1]
    os.makedirs(outputDir, exist_ok=True)

    if len(sys.argv) >= 3:
        psychodrivePath = sys.argv[2]
    else:
        psychodriveBin = "psychodrive"
        if os.name == 'nt':
            psychodriveBin += ".exe"
        psychodrivePath = os.path.join(baseDir, psychodriveBin)

    tasks = discoverTasks(psychodrivePath)

    numThreads = multiprocessing.cpu_count()
    print(f"cooking {len(tasks)} files on {numThreads} threads...")

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=numThreads) as executor:
        futures = {executor.submit(cookOne, psychodrivePath, c, v, outputDir): (c, v) for c, v in tasks}
        for future in concurrent.futures.as_completed(futures):
            charName, version = futures[future]
            outFile = future.result()
            if outFile and os.path.exists(outFile):
                results.append((charName, version, outFile))
                print(".", end="", flush=True)
            else:
                print("x", end="", flush=True)

    print(f"\ncooked {len(results)} files")

if __name__ == "__main__":
    main()
