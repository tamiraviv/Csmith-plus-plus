% rebase('layout.tpl', title=title, year=year)

 
	<h2>Compiler Tests</h2>
	<a  href="{{"/testFIle?testId="+ testId}}">Test File</a> 
<table Style="margin-top: 10px;" class="table table-bordered">
    <thead>
        <th>Name</th>
        <th>Status</th>
        <th>Compiler Status</th>
        <th>Compiler Time</th>
        <th>Program Status</th>
        <th>Program Time</th>
        <th>Program Result</th>
    </thead>
    <tbody>
    %for compilerTest in compilerTests:
        <tr>
            <td class="col-xs-3">{{compilerTest['name']}}</td>
            <td class="col-xs-3">{{compilerTest['status'].name}}</td>
            <td class="col-xs-3">
			% if compilerTest['compiler_status'].value == 3:
				<a  href="{{"/compilerTestError?testId=" + compilerTest['testId']+"&compilerId="+ str(compilerTest['compilerId'])}}">{{compilerTest['compiler_status'].name}}
			%end
			% if compilerTest['compiler_status'].value != 3:
				{{compilerTest['compiler_status'].name}}
			%end
			</td>
            <td class="col-xs-3">{{compilerTest['compiler_time']}}</td>
            <td class="col-xs-3">{{compilerTest['progrem_status'].name}}</td>
            <td class="col-xs-3">{{compilerTest['progrem_time']}}</td>
            <td class="col-xs-3">{{compilerTest['progrem_result']}}</td>
        </tr>
	
        %end
    </tbody>
</table>
