#!/usr/bin/python

import os
import subprocess
import json

scriptPath = os.path.abspath(__file__)
scriptDir = os.path.dirname(scriptPath)
testDir = os.path.join(scriptDir, "dumps")

psychodriveBin = "psychodrive"

if os.name == 'nt':
    psychodriveBin += ".exe"

psychodrivePath = os.path.join(scriptDir, "..", psychodriveBin)

testResults = []

for root, dirs, files in os.walk(testDir):
    for file in files:
        if ".json" in file:
            testPath = os.path.join(root, file)
            charVersion = os.path.basename(root)
            print('running test', testPath, charVersion)
            result = subprocess.run([psychodrivePath, 'rundump', testPath, charVersion], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            newTestResult = {}
            newTestResult['testName'] = file
            newTestResult['testVersion'] = charVersion
            newTestResult['finished'] = False
            newTestResult['errorTypes'] = []

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
            testResults.append(newTestResult)

testResults = sorted(testResults, key=lambda x:x['testName'])

with open(os.path.join(scriptDir, "results.json"), "w") as outFile:
    outFile.write(json.dumps(testResults, default=str, indent=4, ensure_ascii=False))
    outFile.close()