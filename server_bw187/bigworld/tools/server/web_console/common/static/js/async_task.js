/**
 * This module provides some glue to make it easier to manage server-side
 * AsyncTasks (see pycommon/async_task.py).  The idea is to use this module in
 * conjunction with the server-side AsyncTask glue in
 * web_console/common/util.py.  For an example of how to use this client-side
 * code, check out log_viewer/static/js/query.js
 */

/* namespace */ var AsyncTask =
{
	s_tasks: [],

	AsyncTask: function( onID, onUpdate, onNoUpdate,
						 onFinished, sleepTime, blocking, pollURL, stopURL )
	{
		this.id = -1;
		this.terminated = false;

		this.pollURL = pollURL || "/poll";
		this.stopURL = stopURL || "/terminate";
		this.onID = onID;
		this.onUpdate = onUpdate;
		this.onNoUpdate = onNoUpdate;
		this.onFinished = onFinished;
		this.sleepTime = sleepTime;
		this.blocking = blocking;

		// Start this AsyncTask by sending an HTTP request.  If the request
		// comes back with errors, onError() will be called
		this.start = function( url, params, onError )
		{
			var This = this;

			var onSuccess = function( reply )
			{
				This.id = reply.id;
				This.follow();

				if (This.onID)
				{
					This.onID( This.id );
				}
			};

			Ajax.call( url, params, onSuccess, onError );
		}

		// Use this instead of start() when the AsyncTask has already been
		// started and you know the ID
		this.setID = function( id )
		{
			this.id = id;
		}

		this.follow = function()
		{
			var This = this;

			var poll = function()
			{
				if (!This.terminated)
				{
					Ajax.call( This.pollURL,
							   {id: This.id, blocking: This.blocking},
							   onRecv );
				}
			}

			var onRecv = function( reply )
			{
				if (This.terminated)
					return;

				var finished = false;
				var warnings = [];

				if (reply.status == "updated")
				{
					for (var i in reply.updates)
					{
						var state = reply.updates[i][0], data = reply.updates[i][1];

						if (state == "finished")
						{
							finished = true;
							if (This.onFinished)
								This.onFinished();
						}
						else if (state == "__warning__")
						{
							warnings.push( data );
						}
						else if (This.onUpdate)
							This.onUpdate( state, data );
					}
				}
				else
				{
					if (This.onNoUpdate)
						This.onNoUpdate();
				}

				if (warnings.length > 0)
				{
					Util.errorWindow( warnings );
				}

				if (!finished && !This.terminated)
				{
					if (This.sleepTime)
						window.setTimeout( poll, This.sleepTime );
					else
						poll();
				}
			}

			poll();
		}

		this.terminate = function()
		{
			this.terminated = true;
			for (var i in AsyncTask.s_tasks)
				if (AsyncTask.s_tasks[i] == this)
					AsyncTask.s_tasks.splice( i, 1 );

			Ajax.call( this.stopURL, {id: this.id} );
		}

		AsyncTask.s_tasks.push( this );
	},

	get: function( id )
	{
		for (var i in AsyncTask.s_tasks)
			if (AsyncTask.s_tasks[i].id == id)
				return AsyncTask.s_tasks[i];
		return null;
	},


	// Event handler for page unload to terminate all in-progress tasks
	cleanup: function( e )
	{
		for (var i in AsyncTask.s_tasks)
		{
			var task = AsyncTask.s_tasks[i];
			if (task.id != -1 && !task.terminated)
				task.terminate();
		}
	}
};

window.addEventListener( "unload", AsyncTask.cleanup, true );
