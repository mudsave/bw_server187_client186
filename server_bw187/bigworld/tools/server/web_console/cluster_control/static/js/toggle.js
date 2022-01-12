/* namespace */ var Toggle =
{
	timerid: -1,
	layout: new Object(),
	lastUpdate: null,

	// Populates the table on the page with info from an update from the server
	display: function()
	{
		for (var i in Toggle.pnames)
		{
			var dead = 0, running = 0, registered = 0, nomachine = 0;

			for (var j in Toggle.layout)
			{
				var mname = Toggle.layout[j][0];
				var pname = Toggle.layout[j][1];
				var status = Toggle.layout[j][3];

				if (pname == Toggle.pnames[i])
					eval( status + "++" );
			}

			// If see a "nomachine" status, this update is bogus (and
			// won't be the last) so it's OK to throw it away.
			if (nomachine > 0)
			{
				$(Toggle.pnames[i] + "_dead").innerHTML = "?";
				$(Toggle.pnames[i] + "_running").innerHTML = "?";
				$(Toggle.pnames[i] + "_registered").innerHTML = "?";
				continue;
			}

			// Color in the row header with the leftmost non-zero entry
			var style;
			if (dead > 0)
				style = "color: red";
			else if (running > 0)
				style = "color: orange";
			else if (registered > 0)
				style = "color: green";
			else
				continue;

			$(Toggle.pnames[i] + "_dead").innerHTML = dead;
			$(Toggle.pnames[i] + "_running").innerHTML = running;
			$(Toggle.pnames[i] + "_registered").innerHTML = registered;
			$(Toggle.pnames[i] + "_header").setAttribute( "style", style );
		}
	},

	follow: function()
	{
		var onUpdate = function( state, data )
		{
			if (state == "status")
			{
				Toggle.layout = data.layout;
				Toggle.display();
				Toggle.lastUpdate = data.layout;
			}
		};

		var onFinished = function()
		{
			// Figure out whether or not it actually worked
			var success = true;
			if (Toggle.lastUpdate != null)
			{
				for (var i in Toggle.lastUpdate)
				{
					if ((Toggle.action == "start" || Toggle.action == "restart") &&
						Toggle.lastUpdate[i][3] == "dead")
					{
						success = false;
					}

					if (Toggle.action == "stop" &&
						Toggle.lastUpdate[i][3] != "dead")
					{
						success = false;
					}
				}
			}

			if (success ||
				!confirm( "Could not " + Toggle.action + " the server. " +
						  "Do you want to view the log output?" ))
			{
				window.location = "procs?user=" + Toggle.user;
			}
			else
			{
				var q = new Query.Query();
				q.form.serveruser = Toggle.user;
				q.form.showMask = q.SHOW_ALL & ~q.SHOW_USER & ~q.SHOW_PID;
				window.location = q.toURL( true );
			}
		};

		var task = new AsyncTask.AsyncTask( null, onUpdate,
											null, onFinished, 500, false );

		task.setID( Toggle.id );
		task.follow();
	}
};
