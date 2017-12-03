% rebase('layout.tpl', title=title, year=year)

<h2>Query DB</h2>

<form id="runQuery" action="/runQuery" method="get" enctype="multipart/form-data">
  <div class="form-group">
    <textarea class="form-control" name="query" id="exampleTextarea" rows="10" style="min-width: 50%">{{query}}</textarea>
	
  </div>
  <div class="checkbox">
		<label><input type="checkbox"  id="checkbox" name="toCSV" value="toCSV">To Csv</label>
	</div>
  <button type="submit" class="btn btn-primary">Run query</button><small> * Use 'limit' if you suspect a query will return a long table </small>
</form>
<br/>
<table Style="margin-top: 10px;" class="table table-bordered">
    <thead>
	%for columnName in columnNames:
        <th>{{columnName}}</th>
	%end
    </thead>
    <tbody>
    %for row in rows:
        <tr>
			%for field in row:
				<td>{{field}}</td>
			%end
		</tr>
     %end
    </tbody>
</table>
