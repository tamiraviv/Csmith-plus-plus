import uuid
from enum import Enum

class Compiler(object):
    def __init__(self,id,name,command):
        self.id = id
        self.name = name
        self.command = command

class Test(object):
    generationError=""
    hasResultConflict = 0
    testFile = ""
    errorDesc = ""
    def __init__(self):
        self.id = uuid.uuid4()
        self.compilerTests = []
        self.status = TestStatus.Genration
        self.result = TestResult.Uncompleted
        self.generation_time = 0.0;

class CompilerTest(object): 
    def __init__(self,test,compiler):
        self.test = test
        self.compiler = compiler
        self.status = CompilerTestStatus.Compiling 
        self.compilerResult = ProgremResult(ProgremStatus.Uncompleted,-1,"",-1)
        self.progremResult = ProgremResult(ProgremStatus.Uncompleted,-1,"",-1)


class ProgremResult(object): 
    result = -1
    error = ""
    def __init__(self,status,result,error,time):
        self.status = status
        self.result = result
        self.error = error
        self.time = time

class TestStatus(Enum):
    Genration = 0
    CompilerTests = 1
    Completed = 2
    Error = 3

class CompilerTestStatus(Enum):
    Compiling = 0
    RunningProgrem = 1
    Completed = 2

class ProgremStatus(Enum):
    Uncompleted = 0
    TimeOut = 1
    Crash = 2
    Error = 3
    Success = 4

class TestResult(Enum):
    Uncompleted = 0
    Warning = 1
    Error = 2
    Success = 3
    GenrationBug = 4
