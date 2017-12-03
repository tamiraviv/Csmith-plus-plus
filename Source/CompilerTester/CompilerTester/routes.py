"""
Routes and views for the bottle application.
"""

from bottle import route, view, response
from datetime import datetime
from BL import *
from DAL import *
import bottle
from uuid import UUID
import csv
import io

app = bottle.Bottle()

@route('/')
@route('/home')
@view('index')
def home():
    """Renders the home page."""
    stats=GetTestStats()
    stats["year"] = 2016
    return stats

@route('/start')
def start():
    NewRun()
    ExecuteTestsAsync()
    return bottle.redirect('/home')

@route('/runSingleTest')
def runSingleTest():
    RunTestAsync("")
    return bottle.redirect('/home')



@route('/stop')
def stop():
    StopRun()
    return bottle.redirect('/home')

@route('/tests')
@view('tests')
def tests():
    tests=[]
    page=0
    pagingUrl ="/tests?"  
    if 'page' in bottle.request.GET and validate_int(bottle.request.GET['page']):
        page=int(bottle.request.GET['page'])
    if 'result' in bottle.request.GET and validate_int(bottle.request.GET['result']):
        pagingUrl=pagingUrl+"result="+bottle.request.GET['result']+"&"
        tests=GetTestsByTestResult(page,bottle.request.GET['result'])
    else:
        tests=GetTests(page) 
    pagingUrl=pagingUrl+"page="

    return dict(
        pagingUrl=pagingUrl,
        page=page,
        title='Tests',
        tests=tests,
        year=2016
    )



@route('/compilerTests')
@view('compilerTests')
def compilerTests():
    compilerTests=[]
    testId=""
    if 'id' in bottle.request.GET and validate_uuid4(bottle.request.GET['id']):
        testId=bottle.request.GET['id']
        compilerTests=GetCompilerTests(bottle.request.GET['id'])   
    return dict(
        title='Compiler Tests',
        testId=testId,
        compilerTests=compilerTests,
        year=2016
    )

@route('/compilerTestError')
@view('compilerTestError')
def compilerTestError():
    error=""
    if 'testId' in bottle.request.GET and 'compilerId' in bottle.request.GET and validate_uuid4(bottle.request.GET['testId']) and validate_int(bottle.request.GET['compilerId']):
        error=GetCompilerTestError(bottle.request.GET['testId'],bottle.request.GET['compilerId'])   
    return dict(
        title='Compiler Test Error',
        error=error,
        year=2016
    )

@route('/TestError')
@view('TestError')
def TestError():
    error=""
    if 'testId' in bottle.request.GET and validate_uuid4(bottle.request.GET['testId']):
        error=GetTestError(bottle.request.GET['testId'])   
    return dict(
        title='Test Error',
        error=error,
        year=2016
    )

@route('/testFIle')
@view('testFIle')
def testFIle():
    file=""
    if 'testId' in bottle.request.GET and validate_uuid4(bottle.request.GET['testId']):
        file=GetTestFileContent(bottle.request.GET['testId'])   
    return dict(
        title='Compiler Test Error',
        file=file,
        year=2016
    )


@route('/compilers')
@view('compilers')
def compilers():
    compilers=GetCompilers()
    return dict(
        title='Compilers',
        compilers=compilers,
        year=2016
    )

g_columnNames = []
g_rows = []
g_query =''
@route('/queryDB')
@view('queryDB')
def queryDB():
    displayResult = bottle.request.GET['displayResults']
    global g_columnNames
    global g_rows
    global g_query
    if displayResult == 'true':
        return dict(
            title='query DB',
            year=2016,
            columnNames = g_columnNames,
            rows = g_rows,
            query = g_query
        )
    else:
        return dict(
            title='query DB',
            year=2016,
            columnNames = [],
            rows = [],
            query = ''
        )

@route('/runQuery')
def runParam():
    query = bottle.request.GET['query']
    if 'toCSV' in bottle.request.GET:
        return runQueryToCSV(query);
    else:
        result = RunQuery(query)
        global g_columnNames
        global g_rows
        global g_query
        g_columnNames = result['columnNames']
        g_rows = result['rows']
        g_query = query
        return bottle.redirect('/queryDB?displayResults=true')
 
def runQueryToCSV(query): 
    result = RunQuery(query) 
    columnNames = result['columnNames']
    rows = result['rows']  
    response.set_header('Content-type', 'text/csv')
    response.set_header('Content-Disposition', 'attachment; filename=query.csv') 
    with io.StringIO() as f:
        writer = csv.writer(f)
        writer.writerow(columnNames)
        writer.writerows(rows)
        return bytes( f.getvalue(), "UTF-8") 
        
    


@route('/contact')
@view('contact')
def contact():
    """Renders the contact page."""
    return dict(
        title='Contact',
        message='Your contact page.',
        year=datetime.now().year
    )

@route('/about')
@view('about')
def about():
    """Renders the about page."""
    return dict(
        title='About',
        message='Your application description page.',
        year=datetime.now().year
    )

@app.post('/merge') 
@route('/merge')
def merge():
    path = bottle.request.GET['path']
    MergeData(path)
    return bottle.redirect('/home')

@app.post('/runFile') 
@route('/runFile', method='POST')
def runFile():
    file = bottle.request.files.get('file')
    if file:
        RunTestFromFileAsync(file)
    return bottle.redirect('/home')



@route('/runParam')
def runParam():
    path = bottle.request.GET['param']
    RunTestAsync(" "+path)
    return bottle.redirect('/home')


def validate_uuid4(uuid_string):
 
    try:
        val = UUID(uuid_string, version=4)
    except ValueError:
        # If it's a value error, then the string 
        # is not a valid hex code for a UUID.
        return False
 
    return True

def validate_int(s):
    try: 
        int(s)
        return True
    except ValueError:
        return False