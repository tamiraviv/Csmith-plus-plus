

from DM import *
import time
import uuid
import datetime
import os
import sqlite3

root_directory = os.path.dirname(__file__)
db_path = os.path.join(root_directory, 'db.sqlite3')
db_timeout = 15

def create_db():
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:

        #conn.executescript('drop table tests; drop table runs; drop table compilers; drop table compilerTests')

        conn.executescript('''
            CREATE TABLE IF NOT EXISTS runs (
                id  TEXT PRIMARY KEY,
                start INTEGER,
                end INTEGER,
                is_active INTEGER
            );
            CREATE TABLE IF NOT EXISTS compilers (
                id  INTEGER PRIMARY KEY,
                name TEXT,
                command TEXT
            );
            CREATE TABLE IF NOT EXISTS tests (
                id TEXT PRIMARY KEY,
                start_time INTEGER,
                status INTEGER,
                result INTEGER,
                test_file TEXT,
                has_result_conflict INTEGER,
                error_desc TEXT,
                generation_time REAL,
                generation_error TEXT
            );

            CREATE TABLE IF NOT EXISTS compilerTests (
                test_id TEXT REFERENCES tests (id),
                compiler_id INTEGER REFERENCES compilers (id),
                status  INTEGER,
                compiler_status INTEGER,
                compiler_result  BLOB,
                compiler_error TEXT,
                compiler_time REAL,
                progrem_status INTEGER,
                progrem_result  BLOB,
                progrem_error TEXT,
                progrem_time REAL
            ); 
            DELETE FROM compilers where id=1;
            DELETE FROM compilers where id=2;
            DELETE FROM compilers where id=3;
            DELETE FROM compilers where id=4;
            INSERT OR IGNORE INTO compilers VALUES (1, 'gcc','g++ -std=c++11 %s -w -m32 -o %s');
            INSERT OR IGNORE INTO compilers VALUES (2, 'visual c++', 'vcvars32.bat & cl  %s /w /link /out:%s');
            INSERT OR IGNORE INTO compilers VALUES (3, 'clang','clang -std=c++11 %s -w -m32 -o %s');
            INSERT OR IGNORE INTO compilers VALUES (4, 'Intel  c++', 'iclvars.bat intel64 & icl  %s /w /link /out:%s');''')

        try:
            conn.executescript("""
                                    ALTER TABLE tests  
                                    ADD generation_error TEXT;
                                    ALTER TABLE tests  
                                    ADD has_result_conflict INTEGER;"""
                              )
        except:
                pass


def InsertTest(test):
      with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        conn.execute('INSERT INTO tests (id, status, result,test_file,start_time,has_result_conflict,generation_error) VALUES (?, ?, ?, ?, ?, ?, ?)', (str(test.id), test.status.value, test.result.value, test.testFile,int(time.time()),int(test.hasResultConflict),test.generationError))

def UpdateTest(test):
      with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        conn.execute('UPDATE tests SET status=?, result=?,test_file=?,generation_time=?,error_desc=?,has_result_conflict=?,generation_error=? WHERE id=?', (test.status.value, test.result.value, test.testFile, test.generation_time,test.errorDesc ,int(test.hasResultConflict),test.generationError,str(test.id)))


def InsertCompilerTest(compilerTest):
      with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        conn.execute('INSERT INTO compilerTests (test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time) VALUES (?, ?, ?,?, ?, ?,?, ?, ?, ?, ?)', (str(compilerTest.test.id),compilerTest.compiler.id, compilerTest.status.value,compilerTest.compilerResult.status.value ,compilerTest.compilerResult.result,compilerTest.compilerResult.error,compilerTest.compilerResult.time, compilerTest.progremResult.status.value ,compilerTest.progremResult.result,compilerTest.progremResult.error,compilerTest.progremResult.time))

def UpdateCompilerTest(compilerTest):
      with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        conn.execute('UPDATE compilerTests SET status=?, compiler_status=?, compiler_result=?, compiler_error=?, compiler_time=?, progrem_status=?, progrem_result=? , progrem_error=? , progrem_time=? WHERE test_id=? and compiler_id=?', (compilerTest.status.value,compilerTest.compilerResult.status.value ,compilerTest.compilerResult.result,compilerTest.compilerResult.error, compilerTest.compilerResult.time, compilerTest.progremResult.status.value ,compilerTest.progremResult.result,compilerTest.progremResult.error,compilerTest.progremResult.time,str(compilerTest.test.id),compilerTest.compiler.id))

def GetTestStats():
      with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        runs = conn.execute('SELECT count(*) FROM runs WHERE is_active = 1').fetchone()[0]
        all = conn.execute('SELECT count(*) FROM tests').fetchone()[0]
        warning = conn.execute('SELECT count(*) FROM tests WHERE result = ?', (TestResult.Warning.value,)).fetchone()[0]
        error = conn.execute('SELECT count(*) FROM tests WHERE result = ?', (TestResult.Error.value,)).fetchone()[0]
        genrationBug = conn.execute('SELECT count(*) FROM tests WHERE result = ?', (TestResult.GenrationBug.value,)).fetchone()[0]
        generationTime = conn.execute('SELECT round(sum(generation_time)/60/60, 2) FROM tests WHERE generation_time not null').fetchone()[0]
        if generationTime is None:
            generationTime = 0.0
        compilationTime = conn.execute('SELECT round(sum(max(compiler_time, 0))/60/60 , 2) FROM compilerTests').fetchone()[0]
        if compilationTime is None:
            compilationTime = 0.0
        executionTime = conn.execute('SELECT round(sum(max(progrem_time, 0))/60/60 , 2) FROM compilerTests').fetchone()[0]
        if executionTime is None:
            executionTime = 0.0
        CPUTotalTime = generationTime + compilationTime + executionTime
        return {"runs":runs,"all":all,"warning":warning,"error":error,"genrationBug":genrationBug,
                "CPUTotalTime":round(CPUTotalTime, 2), 
                "generationTimePercentage": round((generationTime/(CPUTotalTime if CPUTotalTime > 0 else 1.0))*100),
                "compilationTimePercentage": round((compilationTime/(CPUTotalTime if CPUTotalTime > 0 else 1.0))*100),
                "executionTimePercentage": round((executionTime/(CPUTotalTime if CPUTotalTime > 0 else 1.0))*100)}

def RunQuery(query):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        cur = conn.execute(query)
        return { "rows" :cur.fetchall() ,
                "columnNames" : [tuple[0] for tuple in cur.description]}

def NewRun():    
    if not HasActiveRun():  
        with sqlite3.connect(db_path,timeout=db_timeout) as conn:
            conn.execute('INSERT INTO runs (id, start, end,is_active) VALUES (?, ?, -1, 1)', (str(uuid.uuid4()), int(time.time())))

def StopRun():
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        conn.execute('UPDATE runs SET is_active=0, end=? WHERE is_active=1', (int(time.time()),))

def HasActiveRun():
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
       row = conn.execute('SELECT count(*) FROM runs WHERE is_active = 1').fetchone()
       return row[0]

def GetTests(pageNumber):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        tests = [{
            'id'  : id,
            'startTime' : datetime.datetime.fromtimestamp(int(startTime)).strftime('%d.%m.%Y %H:%M:%S'),
            'testFile'   : testFile,
            'status'     : TestStatus(int(status)),
            'result'     : TestResult(int(result)),
            'errorDesc'  : errorDesc,
        } for id, startTime,testFile, status,result,errorDesc in conn.execute('SELECT id, start_time, test_file, status,result,error_desc FROM tests ORDER BY start_time DESC LIMIT 101 OFFSET ?',(pageNumber * 100,)).fetchall()]
        return tests

def GetTestsByTestResult(pageNumber,result):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        tests = [{
            'id'  : id,
            'startTime' : datetime.datetime.fromtimestamp(int(startTime)).strftime('%d.%m.%Y %H:%M:%S'),
            'testFile'   : testFile,
            'status'     : TestStatus(int(status)),
            'result'     : TestResult(int(result)),
            'errorDesc'  : errorDesc,
        } for id, startTime,testFile, status,result,errorDesc in conn.execute('SELECT id, start_time, test_file, status,result,error_desc FROM tests WHERE result=? ORDER BY start_time DESC LIMIT 101 OFFSET ?',(result,pageNumber * 100)).fetchall()]
        return tests

def GetCompilerTests(test_id):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        compilerTests = [{
            'testId'  : test_id,
            'compilerId'  : compiler_id,
            'name'  : name,
            'status' : CompilerTestStatus(int(status)),
            'compiler_status'   : ProgremStatus(int(compiler_status)),
            'compiler_time'     : compiler_time,
            'progrem_status'   : ProgremStatus(int(progrem_status)),
            'progrem_result'   : progrem_result,
            'progrem_time'     : progrem_time,
        } for test_id,compiler_id,name, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time in conn.execute('SELECT test_id,compiler_id,name, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time FROM compilerTests INNER JOIN compilers ON compilerTests.compiler_id = compilers.id WHERE test_id=?',(test_id,)).fetchall()]
        return compilerTests


def GetCompilerTestError(testId,compilerId):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        return conn.execute('SELECT compiler_error FROM compilerTests WHERE test_id = ? AND compiler_id = ? ',(testId,compilerId)).fetchone()[0]

def GetTestError(testId):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        return conn.execute('SELECT generation_error FROM tests WHERE id = ? ',(testId, )).fetchone()[0]

def GetTestFile(testId):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        return conn.execute('SELECT test_file FROM tests WHERE id = ?',(testId,)).fetchone()[0]


def GetCompilers():
    with sqlite3.connect(db_path,timeout=db_timeout) as conn:
        compilers = [Compiler(id,name,command) for id, name, command in conn.execute('SELECT id, name, command FROM compilers').fetchall()]
        return compilers


def merge_db(path):  
    if os.path.exists(os.path.join(path, 'db.sqlite3')):
        with sqlite3.connect(os.path.join(path, 'db.sqlite3'),timeout=db_timeout) as conn:
            GetTestsFromDB(conn)
            GetCompilerTestsFromDB(conn)

        

def GetTestsFromDB(conn):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn1:
        for id, status, result,test_file,start_time in conn.execute('SELECT id, status, result,test_file,start_time FROM tests').fetchall():
            InsertTestFromDB(conn1,id, status, result,test_file,start_time)


def GetCompilerTestsFromDB(conn):
    with sqlite3.connect(db_path,timeout=db_timeout) as conn1:
        for test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time in conn.execute('SELECT test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time FROM tests').fetchall():
            InsertCompilerTestFromDB(conn1,test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time)

def InsertCompilerTestFromDB(conn,test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time):
    conn.execute('INSERT INTO compilerTests (test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time) VALUES (?, ?, ?,?, ?, ?,?, ?, ?, ?, ?)', (test_id, compiler_id, status,compiler_status,compiler_result,compiler_error,compiler_time,progrem_status,progrem_result,progrem_error,progrem_time))


def InsertTestFromDB(conn,id, status, result,test_file,start_time):
    conn.execute('INSERT INTO tests (id, status, result,test_file,start_time) VALUES (?, ?, ?, ?, ?)', (id, status, result,test_file,start_time))

