% rebase('layout.tpl', title=title, year=year)

<h2>Compilers</h2>

<table class="table table-bordered">
    <thead>
        <th>id</th>
        <th>Name</th>
        <th>command</th>
    </thead>
    <tbody>
    %for compiler in compilers:
        <tr>
            <td class="col-xs-3">{{compiler.id}}</td>
            <td class="col-xs-3">{{compiler.name}}</td>
            <td class="col-xs-3">{{compiler.command}}</td>
        </tr>
        %end
    </tbody>
</table>
