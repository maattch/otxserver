function onRecord(current, old, cid)
	db.executeQuery("INSERT INTO `server_record` (`record`, `timestamp`) VALUES (" .. current .. ", " .. os.time() .. ")")
	addEvent(doBroadcastMessage, 150, "New record: " .. current .. " players are logged in.", MESSAGE_STATUS_DEFAULT)
end
