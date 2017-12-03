% rebase('layout.tpl', title='Home', year=year)

<div class="jumbotron">
    <h1>Compiler Tester</h1>
	% if not runs:
		<p class="lead">Start Test</p>
		<div>
		<a href="/start" class="btn btn-primary btn-large">Start</a>
		<a href="/runSingleTest" class="btn btn-primary btn-large">Run Single Test</a>
		</div>
		
	% end
	% if runs:
		<p class="lead">Tests are Running</p>
		<p><a href="/stop" class="btn btn-primary btn-large">Stop</a></p>
	% end

	<h4 >Total tests CPU time: <b>{{ CPUTotalTime }}</b> hours. ( {{ generationTimePercentage }}% Test code generation, {{ compilationTimePercentage }}% Compilation, {{ executionTimePercentage }}% Execution)</h4>

</div>

% if not runs:
<div class="row">
    <div class="col-md-4">
<h2>Merge</h2>
<form id="merge" action="/merge" method="get" enctype="multipart/form-data">
    <div class="form-group">
        <label for="path">Path: </label>
        <input type="directory" class="form-control" name="path" />
    </div>
    <button type="submit" class="btn btn-default">Merge</button>
</form>
 </div>
	 <div class="col-md-4">
<h2>Run File</h2>
	  <form id="runFile" action="/runFile" method="post" enctype="multipart/form-data">
                <div class="form-group">
                    <label for="image">File: </label>
                    <input type="file" class="form-control" name="file" />
                </div>
                <button type="submit" class="btn btn-default">Run</button>
            </form>
 </div>
     <div class="col-md-4">
<h2>Run With Param</h2>
<form id="runParam" action="/runParam" method="get" enctype="multipart/form-data">
    <div class="form-group">
        <label for="path">Param: </label>
        <input type="text" class="form-control" name="param" />
    </div>
    <button type="submit" class="btn btn-default">Run</button>
</form>
 </div>
</div>
% end
<div class="row">
    <div class="col-md-3">
        <h2>Total</h2>
        <a class="btn btn-default" href="/tests"><h1  Style="margin-top: 10px;">{{ all }}</h1></a>
    </div> 
	<div class="col-md-3">
        <h2>Genration Bug</h2>
       <a class="btn btn-default" href="/tests?result=4"><h1 Style="margin-top: 10px;">{{ genrationBug }}</h1></a>
    </div>
    <div class="col-md-3">
        <h2>Warning</h2>
       <a class="btn btn-default" href="/tests?result=1"><h1 Style="margin-top: 10px;">{{ warning }}</h1></a>
    </div>
    <div class="col-md-3">
        <h2>Error</h2>
       <a class="btn btn-default" href="/tests?result=2"><h1 Style="margin-top: 10px;">{{ error }}</h1></a>
    </div>
	
</div>
<br/>
