#!/usr/bin/python

import os
import subprocess

scriptPath = os.path.abspath(__file__)
scriptDir = os.path.dirname(scriptPath)

runTestsPath = os.path.join(scriptDir, "run_tests.py")
curResultsPath = os.path.join(scriptDir, "current_results.json")
newResultsPath = os.path.join(scriptDir, "new_results.json")

subprocess.run(runTestsPath)

with open(newResultsPath) as file:
    results = file.read()
    file.close()

with open(curResultsPath, "w") as outFile:
    outFile.write(results.replace("newResults", "currentResults", 1))
    outFile.close()

subprocess.run(['git', 'add', curResultsPath])