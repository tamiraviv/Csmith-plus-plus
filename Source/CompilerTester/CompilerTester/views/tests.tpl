% rebase('layout.tpl', title=title, year=year)
 
<h2>Tests</h2> 

<table class="table table-bordered">
    <thead>
        <th>id</th>
        <th>Start Time</th>
        <th>Status</th>
        <th>Result</th>
        <th>File</th>
    </thead>
    <tbody>
    %for test in tests[:min(len(tests),100)]:
        <tr>
            <td class="col-xs-3"><a  href="{{"/compilerTests?id=" + test['id']}}">{{test['id']}}</a></td>
            <td class="col-xs-3">{{test['startTime']}}</td>
            <td class="col-xs-3">
			% if test['status'].value == 3:
				<a  href="{{"/TestError?testId=" + test['id']}}">{{test['status'].name}}</a>
			%end
			% if test['status'].value != 3:
					{{test['status'].name}}
			%end
			</td>
            <td class="col-xs-3">{{test['result'].name}}</td>
            <td class="col-xs-3">
				% if not test['testFile']=='': 
					<a  href="{{"/testFIle?testId="+test['id']}}">Test File</a>
				% end
			</td>
        </tr>
			
		% if test['errorDesc'] != "" and test['errorDesc'] != None:
		 <tr>
			<td colspan="5" class="col-xs-1"><pre>{{test['errorDesc']}}</pre></td>
        </tr>
		%end
        %end
    </tbody>
</table>
<div class="row">
	% if not page==0: 
		<a href="{{pagingUrl + str(page-1)}}" class="btn btn-primary btn-large"><</a>
 
	% end
	% if len(tests)==101: 
		<a href="{{pagingUrl + str(page+1)}}" class="btn btn-primary btn-large">></a>
	% end
</div>
