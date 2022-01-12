<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<?python
  layout_params[ "moduleHeader" ] = "StatGrapher"
?>

<html xmlns="http://www.w3.org/1999/xhtml"
	  xmlns:py="http://purl.org/kid/ns#"
	  py:layout="'../../common/templates/layout.kid'"
	  py:extends="'../../common/templates/common.kid'">

<body>
		<div py:def="moduleContent()">

		<script type="text/javascript" src="/static/js/dojo/dojo.js"/>
			
		<script type="text/javascript">
			PAGE_TITLE = 'Stat Grapher';
			djConfig.baseRelativePath = "/static/js/dojo/";
			dojo.require( "dojo.widget.Dialog" );
		</script>

		<script type="text/javascript" src="/statg/static/js/exception.js"></script>
		<script type="text/javascript" src="/statg/static/js/flashtag.js"></script>
		<script type="text/javascript" src="/statg/static/js/flashserializer.js"></script>
		<script type="text/javascript" src="/statg/static/js/flashproxy.js"></script>
		<script type="text/javascript" src="/statg/static/js/legend.js"></script>
		<script type="text/vbscript" src="/statg/static/js/vbcallback.vbs"></script>

		<table style="height: 100%; width: 100%; overflow: auto" id="graphtable">
		<tr>
			<td id="flashCell" style="width:100%; height: 100%;">
			<div style="height:100%; width:100%; overflow:hidden;">
				<script py:if="debug" type="text/javascript">
				// console.log( "Flash debugging enabled" );
				flashDebug = function( msg )
				{
					document.getElementById( "flashDebugOutput" ).value += msg + "\n";
				}
				</script>
				<script py:if="not debug" type="text/javascript">
				// console.log( "Flash debugging disabled" );
				flashDebug = function() {}
				</script>

				<script type="text/javascript">
				var flashId = new Date().getTime();
				var flashProxy = new FlashProxy( flashId, 'graphDisplay', 
					'/statg/static/swf/javascriptflashgateway.swf' );
				var tag = new FlashTag('/statg/static/swf/graph.swf', 
					'100%', '100%', '7,0,14,0' ); 
				tag.addFlashVar( 'flashId', flashId );
				tag.addFlashVar( 'logName', "$log" );
				tag.addFlashVar( 'user', "$user" );
				tag.addFlashVar( 'profile', '$profile' );
				tag.addFlashVar( 'serviceURL', "$serviceURL" );
				tag.addFlashVar( 'bwdebug', "${debug and 'true' or 'false'}" );
				tag.addFlashVar( 'desiredPointsPerView', "$desiredPointsPerView" );
				tag.addFlashVar( 'minGraphWidth', "$minGraphWidth" );
				tag.addFlashVar( 'minGraphHeight', "$minGraphHeight" );
				tag.setScale( 'noscale' );
				tag.setSalign( 'tl' );
				tag.setAlign( 'middle' );
				tag.setWidth( '100%' );
				tag.setHeight( '100%' );
				//tag.setWmode( 'transparent' );
				tag.setId( 'graphDisplay' );
				tag.write( document );
				</script>

			</div>
			</td>

			<td id="legendCell" class = "legend" style="display:none; height: 100%; overflow: auto;">
				<div id="legendDiv" style="width: 100%; height: 100%; overflow: auto; padding: 2px;">
				</div>
			</td>
		</tr>
		</table>
		<div dojoType="dialog" id="stat_pref_edit_dialog" style="display: none;">
		<div id="stat_pref_edit_contents" class="stat_pref_edit"/>
		</div>
		<div py:if="debug">
			<textarea cols="80" rows="24" id="flashDebugOutput" editable="no"></textarea><br/>
			<button onClick="document.getElementById( 'flashDebugOutput' ).value = ''; return true;">clear</button>
		</div>
		</div>

</body>

</html>
