<?php
	
/*
Feedign a file to this script:
https://medium.com/@petehouston/upload-files-with-curl-93064dcccc76
E.g. use curl to send POST request, setting the variables "auth", "pic" and "cam" accordingly.
curl -F 'fileX=@/path/to/fileX' -F 'fileY=@/path/to/fileY' ... http://localhost/upload
*/

	header("Content-Type: text/plain");

	include("functions.php");
	LoadDefaults();
	//LoadPics();

	if ( !(array_key_exists("auth", $_POST) && array_key_exists("cam", $_POST) && array_key_exists("picname", $_POST) && array_key_exists("pic", $_FILES)))
	{
		echo "this page is to be used with HTTPS POST requests only and expects an authorization token 'auth', cam identifier 'cam', proposed filename 'picname' and a file 'pic' provided with encoding type multipart/form-data.\r\n";
		exit;
	}
	
	// validate authentication token
	$toks = array(
		"token1",);
	if ( !in_array($_POST['auth'], $toks) )
	{
		echo "authentication failed.\r\n";
		exit;
	}

	// check whether upload was successful and move file to it's place
	$pic = $_FILES['pic'];

	// upload error?
	if ( $pic['error'] != UPLOAD_ERR_OK )
	{
		echo "file upload failed: ".$pic['error']."\r\n";
		exit;
	}

	$tempName = $pic['tmp_name'];
	$origName = basename($_POST['picname']);

	if ( ! move_uploaded_file($tempName, $CamPicDir."/".$origName) )
	{
		echo "file move to definitive place failed.\r\n";
	}
	else
	{
		echo "upload of ".$origName." successful\r\n";
	}

	// remove files if there are too many
	$images = getImageFileNames($CamPicDir);
	// remove files older than x days (images are sorted by camera name prefix...)
	$cutTime = time() - 10 * 24 * 60 * 60;
	foreach ( $images as $img )
	{
		$f = $CamPicDir."/".$img;
		if ( filectime($f) < $cutTime )
			unlink($f);
	}
	/*
	$keep = 20;
	foreach ( $images as $img )
	{
		if ( $keep-- > 0 )
			continue;
		unlink($CamPicDir."/".$img);
	}
	*/
?>