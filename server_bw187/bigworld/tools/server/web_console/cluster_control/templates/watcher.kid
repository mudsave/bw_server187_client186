<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<?python
  layout_params[ "pageHeader" ] = "Watchers"
?>

<html xmlns="http://www.w3.org/1999/xhtml"
	  xmlns:py="http://purl.org/kid/ns#"
	  py:layout="'layout.kid'"
	  py:extends="'common.kid'">
	
<div py:def="pageContent()">
	
	<script type="text/javascript">
		PAGE_TITLE = 'Watcher Listing';
	</script>

	<?python
	  import os
	  from web_console.common.util import alterParams

	  user = m.cluster.getUser( p.uid )
	  pathSplit = wd.path.split( "/" )
	?>

	
	<table class="watcher" width="100%">
	<th class="heading">Watcher Values for 
			<span py:if="not wd.path" py:content="p.label()"/>
			<span py:if="wd.path">
			<a href="${alterParams( path='' )}">${p.label()}</a>
			<span py:for="i in xrange( len( pathSplit ) - 1 )">
				/<a href="${alterParams( path='/'.join( pathSplit[:i+1] ) )}">${pathSplit[i]}</a>
			</span>
			/ ${pathSplit[-1]}
			</span>
		</th>
	</table>

	<div py:if="wd.isDir()">

		<script type="text/javascript" src="/static/js/table.js"/>

		<?python
		  children = wd.getChildren()
		  subdirs = [c for c in children if c.isDir()]
		  watchers = [c for c in children if not c.isDir()]

		?>

		<table class="watcher">
			<tr>
			<th class="heading" colspan="3">
			</th>
			</tr>
			<tr py:if="wd.path">
				<td colspan="3">
				<a href="${alterParams( path=os.path.dirname( wd.path ) )}">..</a>
				</td>
			</tr>
	
			<tr py:for="dir in subdirs" class="watcherrow">
				<td colspan="3">
					<a href="${alterParams( path=dir.path )}">
						${os.path.basename( dir.path )}
					</a>
				</td>
			</tr>
			<tr py:for="w in watchers" class="watcherrow">
				<td>${os.path.basename( w.path )} </td>
				<td>${w.value}</td>
				<td><a href="${alterParams( path=w.path )}">Edit</a></td>
			</tr>
		</table>
	</div>

	<div py:if="not wd.isDir()">

		<form action="watcher" method="get">
		<table class="watcher">
			<tr><th class="heading" colspan="2">${wd.name}</th></tr>
			<tr>
				<td class="colheader">Existing Value</td>
				<td>${wd.value}</td>
			</tr>
			<tr>
				<td class="colheader">New Value</td>
				<td>
					<input type="hidden" name="machine" value="${m.name}"/>
					<input type="hidden" name="pid" value="${p.pid}"/>
					<input type="hidden" name="path" value="${wd.path}"/>
					<input name="newval" value="${wd.value}" type="text"/>
				</td>
			</tr>
			<tr>
				<td colspan="2" style="text-align:right;">
					<input type="submit" value="Modify"/>
				</td>
			</tr>
		</table>
		
		</form>

	</div>

	<script type="text/javascript">
		if ("${status}" == "False")
		{
			Util.error( "Failed to set value" );
		}
	</script>
	
</div>

</html>
