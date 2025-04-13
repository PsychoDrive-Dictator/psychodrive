#!/usr/bin/python

import os
import subprocess
import json
import sys
from datetime import datetime
import concurrent.futures
import multiprocessing
import time

scriptPath = os.path.abspath(__file__)
scriptDir = os.path.dirname(scriptPath)
testDir = os.path.join(scriptDir, "dumps")

psychodriveBin = "psychodrive"

if os.name == 'nt':
    psychodriveBin += ".exe"

psychodrivePath = os.path.join(scriptDir, "..", psychodriveBin)

resultsPath = os.path.join(scriptDir, "new_results.json")

tests = []

for root, dirs, files in os.walk(testDir):
    for file in files:
        if ".json" in file:
            newTest = {}
            newTest['name'] = file.split('.')[0]
            newTest['filePath'] = os.path.join(root, file)
            newTest['charVersion'] = os.path.basename(root)
            tests.append(newTest)

testsUpToDate = True

resultsModTime = None
if os.path.isfile(resultsPath):
    resultsModTime = os.path.getmtime(resultsPath)

if resultsModTime == None:
    testsUpToDate = False
else:
    if resultsModTime < os.path.getmtime(psychodrivePath):
        testsUpToDate = False
    for test in tests:
        if resultsModTime < os.path.getmtime(test['filePath']):
            testsUpToDate = False

for arg in sys.argv:
    if arg == '--force':
        testsUpToDate = False

if testsUpToDate:
    print("tests up to date! (--force to override)")
    exit(0)

testResults = []

def runTest(test):
    testPath = test['filePath']
    charVersion = test['charVersion']
    print('running test', testPath, charVersion)
    result = subprocess.run([psychodrivePath, 'run_dump', testPath, charVersion], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    newTestResult = {}
    newTestResult['testName'] = test['name']
    newTestResult['testVersion'] = charVersion
    newTestResult['finished'] = False
    newTestResult['errorTypes'] = []
    while 8 > len(newTestResult['errorTypes']):
        emptyErrorType = {}
        emptyErrorType['count'] = 0
        emptyErrorType['firstFrame'] = -1
        emptyErrorType['lastFrame'] = -1
        newTestResult['errorTypes'].append(emptyErrorType)

    for line in result.stderr.splitlines():
        errorLine = line.split(';')
        if errorLine[0] == 'E':

            errorType = int(errorLine[3])
            while errorType + 1 > len(newTestResult['errorTypes']):
                emptyErrorType = {}
                emptyErrorType['count'] = 0
                emptyErrorType['firstFrame'] = -1
                emptyErrorType['lastFrame'] = -1
                newTestResult['errorTypes'].append(emptyErrorType)

            errorFrame = int(errorLine[2])
            if newTestResult['errorTypes'][errorType]['firstFrame'] == -1:
                newTestResult['errorTypes'][errorType]['firstFrame'] = errorFrame
            newTestResult['errorTypes'][errorType]['lastFrame'] = errorFrame
            newTestResult['errorTypes'][errorType]['count'] += 1
        if errorLine[0] == 'F':
            newTestResult['finished'] = True
    return newTestResult

executor = concurrent.futures.ThreadPoolExecutor(max_workers=multiprocessing.cpu_count())

futures = []
futuresNotDone = []

for test in tests:
    futures.append(executor.submit(runTest, test))

while True:
    for f in futures:
        if f.done():
            testResults.append(f.result())
        else:
            futuresNotDone.append(f)
    futures = futuresNotDone
    futuresNotDone = []
    if len(futures) == 0:
        break
    time.sleep(0.1)

testResults = sorted(testResults, key=lambda x:x['testName'])

with open(resultsPath, "w") as outFile:
    outFile.write("var newResults =\n")
    outFile.write(json.dumps(testResults, default=str, indent=4, ensure_ascii=False))
    outFile.close()
