
from subprocess import STDOUT,check_output, TimeoutExpired, CalledProcessError,Popen, PIPE
from DM import *
from shutil import copyfile
import os
from builtins import OSError, Exception, print
import time
from DAL import *
import _thread
import signal
from threading import Timer
from time import sleep
import sys
from distutils.dir_util import copy_tree

THREADS = 4
TIME_OUT = 180
PROGREM_TIME_OUT = 60
GENERATE_COMMAND = "csmith.exe"
TEST_FOLDER = "Temp\\"
ERROR_FOLDER = "Temp\\Error\\"
WARNING_FOLDER = "Temp\\Warning\\"
BUG_FOLDER = "Temp\\Bug\\"
FILE_EXTENSION = ".cpp"

root_directory = os.path.dirname(__file__)


def ExecuteTestsAsync():
    for x in range(THREADS):
        _thread.start_new_thread(ExecuteTests,())
    
def RunTestAsync(param):
   _thread.start_new_thread(RunTest,(param,))

def ExecuteTests():
    try:
        while(HasActiveRun()):  
            RunTest("")
    except Exception as e:
        #print(str(e).encode(sys.stdout.encoding, errors='replace'))
        sleep(5)
        _thread.start_new_thread(ExecuteTests,())
       
    

def RunTestFromFileAsync(path):
    test = Test()
    test.testFile = GetFullPath(GetTempTestFileLocation(test.id))
    path.save(test.testFile, overwrite=True)
    _thread.start_new_thread(RunTestFromFile,(test,path))

def RunTestFromFile(test,path):
    test = GenerateTestFromFile(test,path)
    RunGeneratedTest(test)

def GenerateTestFromFile(test,path): 
    InsertTest(test)
    result = ProgremResult(ProgremStatus.Success,"","",0.0)
    UpdateTest(test)
    for compiler in GetCompilers():
        compilerTest = CompilerTest(test,compiler)
        InsertCompilerTest(compilerTest)
        test.compilerTests.append(compilerTest)
    return test

def RunTest(param):
    test = GenerateTest(param)
    RunGeneratedTest(test)


def RunGeneratedTest(test):
    if test.status != TestStatus.Error:
        test.status = TestStatus.CompilerTests
        UpdateTest(test)
        for compilerTest in test.compilerTests: 
            RunCompilerTest(compilerTest)
        EvaluteTest(test)
        test.errorDesc = GetErrorDesc(test)
        CleanTest(test)
        test.status = TestStatus.Completed
        UpdateTest(test)
    

def GetErrorDesc(test):
    errorDesc = ""
    for compilerTest in test.compilerTests:
        if compilerTest.compilerResult.status == ProgremStatus.Error:
            errorDesc = errorDesc + GetCompilerError(compilerTest)
    if test.hasResultConflict:
        errorDesc=errorDesc+"Conflicted Results!!!"
    return errorDesc


def GetCompilerError(compilerTest):
    errorDesc = compilerTest.compiler.name + ": "
    lines = compilerTest.compilerResult.error.decode().splitlines(True)
    for line in lines:
        if "error" in line.lower():
            errorDesc = errorDesc + line
            break
    return errorDesc

def GenerateTest(param):
    test = Test()
    test.testFile = GetFullPath(GetTempTestFileLocation(test.id))
    InsertTest(test)
    start_time = time.time()
    result = RunProgrem(GENERATE_COMMAND + param)
    test.generationError=result.error;
    if result.status != ProgremStatus.Success:
        test.status = TestStatus.Error
    else:
        test.generation_time = time.time() - start_time
        with open(test.testFile, 'wb') as f:
             f.write(result.result)
    UpdateTest(test)
    for compiler in GetCompilers():
        compilerTest = CompilerTest(test,compiler)
        InsertCompilerTest(compilerTest)
        test.compilerTests.append(compilerTest)
    return test

def RunCompilerTest(compilerTest):
    testLocation = GetCompilerTestLocation(compilerTest.compiler.id)
    testFile = ("%s" + FILE_EXTENSION) % compilerTest.test.id
    testObjFile = "%s.obj" % compilerTest.test.id
    testOutput = "%s.exe" % compilerTest.test.id
    compilerFolder = GetCompilerTestLocation(compilerTest.compiler.id)
    compilerTestLocation = compilerFolder + testFile
    compilerTestObjLocation = compilerFolder + testObjFile
    copyfile(compilerTest.test.testFile,compilerTestLocation)
    compilerTest.compilerResult = RunProgremInCwd(compilerTest.compiler.command % (testFile,testOutput),testLocation)
    UpdateCompilerTest(compilerTest)
    if  compilerTest.compilerResult.status == ProgremStatus.Success:
        compilerTest.status = CompilerTestStatus.RunningProgrem
        compilerTest.progremResult = RunProgremInCwd(os.path.join(root_directory, testLocation + testOutput),testLocation,False,PROGREM_TIME_OUT)
        UpdateCompilerTest(compilerTest)
    compilerTest.status = CompilerTestStatus.Completed
    UpdateCompilerTest(compilerTest)
    RemoveFile(compilerTestLocation)
    RemoveFile(compilerTestObjLocation)
    RemoveFile(compilerFolder + testOutput)





def EvaluteTest(test):
    statusMapping = {ProgremStatus.Uncompleted:0,ProgremStatus.TimeOut:0,ProgremStatus.Crash:0,ProgremStatus.Error:0,ProgremStatus.Success:0}
    compilerStatusMapping = {ProgremStatus.Uncompleted:0,ProgremStatus.TimeOut:0,ProgremStatus.Crash:0,ProgremStatus.Error:0,ProgremStatus.Success:0}
    resultMapping = {}
    for compilerTest in test.compilerTests:
        if compilerTest.progremResult.result != -1:
            if not (compilerTest.progremResult.result in resultMapping):
                resultMapping[compilerTest.progremResult.result] = 0
            resultMapping[compilerTest.progremResult.result] = resultMapping[compilerTest.progremResult.result] + 1
        statusMapping[compilerTest.progremResult.status] = statusMapping[compilerTest.progremResult.status] + 1
        compilerStatusMapping[compilerTest.compilerResult.status] = compilerStatusMapping[compilerTest.compilerResult.status] + 1
    compilerTestsCount = len(test.compilerTests)
    resultCount = len(resultMapping)
    diffrentStatuses = 0
    test.hasResultConflict=resultCount>1;
    if compilerStatusMapping[ProgremStatus.Error] == compilerTestsCount or statusMapping[ProgremStatus.Error] == compilerTestsCount:
        test.result = TestResult.GenrationBug
        return 

    if compilerStatusMapping[ProgremStatus.TimeOut] == compilerTestsCount:
        test.result = TestResult.Warning
        return 

    for key, value in statusMapping.items():
        if value == compilerTestsCount and resultCount <= 1:
            test.result = TestResult.Success
            return 
        if value > 0:
            diffrentStatuses = diffrentStatuses + 1

    if diffrentStatuses == 2 and statusMapping[ProgremStatus.TimeOut] > 0 and resultCount <= 1:
        test.result = TestResult.Warning
        return 

    test.result = TestResult.Error 




def CleanTest(test):
    testFile = ("%s" + FILE_EXTENSION) % test.id
    if test.result == TestResult.Uncompleted or test.result == TestResult.Success:
        os.remove(test.testFile)
        test.testFile = ""
    else:
        folder = GetTestFolderByResult(test.result)
        os.rename(test.testFile,folder + testFile)
        test.testFile = GetFullPath(folder + testFile)

def GetTestFolderByResult(testResult):
    if testResult == TestResult.Warning:
        return WARNING_FOLDER
    elif testResult == TestResult.GenrationBug:
        return BUG_FOLDER
    else:
        return ERROR_FOLDER

def GetTempTestFileLocation(id):
    return (TEST_FOLDER + "\\%s" + FILE_EXTENSION) % id

def GetCompilerTestLocation(compilerId):
    return (TEST_FOLDER + "%s\\") % (compilerId)


def RunProgrem(command):
    return RunProgremInCwd(command,".")

def RunProgremInCwd(command,cwd,shell=False,timeout=TIME_OUT):
    start_time = time.time()
    try:
        output = check_output(command.split(' '),shell=shell, stderr=STDOUT, timeout=timeout,cwd=cwd)
        return ProgremResult(ProgremStatus.Success,output,"",time.time() - start_time)
    except TimeoutExpired as e:
         return ProgremResult(ProgremStatus.TimeOut,-1,"",time.time() - start_time)
    except CalledProcessError as e:
         if e.returncode <= 125:
            return ProgremResult(ProgremStatus.Error,e.returncode,e.output,time.time() - start_time)
         else:
            return ProgremResult(ProgremStatus.Crash,e.returncode,e.output,time.time() - start_time) 


def GetFullPath(path):
     return os.path.join(root_directory, path)

def EnsureDirs():
    EnsureDir(TEST_FOLDER)
    EnsureDir(ERROR_FOLDER)
    EnsureDir(WARNING_FOLDER)
    EnsureDir(BUG_FOLDER)
    for compiler in GetCompilers():
        EnsureDir(GetCompilerTestLocation(compiler.id))

def EnsureDir(path):
    if not os.path.exists(path):
        os.makedirs(path)

def RemoveFile(path):
    if os.path.exists(path):
        os.remove(path)

def GetTestFileContent(testId):
    file = GetTestFile(testId)
    content = ""
    with open(file, 'r') as f:
        data = f.readlines()
        for (number, line) in enumerate(data):
            content = content + ('%d\t%s' % (number + 1, line))
    return content

def MergeData(path):
    if os.path.exists(os.path.join(path, TEST_FOLDER)):
        MergeFiles(path)
    merge_db(path)

def MergeFiles(path):
    EnsureDir(os.path.join(path, ERROR_FOLDER),ERROR_FOLDER)
    EnsureDir(os.path.join(path, WARNING_FOLDER),WARNING_FOLDER)
    EnsureDir(os.path.join(path, BUG_FOLDER),BUG_FOLDER)
        

def MergeDir(path,target):
    if os.path.exists(path):
        copy_tree(path, target)