#ifndef AUTH_TOKEN_H
#define AUTH_TOKEN_H

// Server Base URL Configuration
// Base URL can be:
//   - Domain root: "https://your-server.com"
//   - With path:   "https://your-server.com/cams"
// Do NOT include the endpoint filename (e.g., NOT ".../upload.php")
// Endpoints (/upload.php, /log.php, /ota-download.php, /ota-confirm.php) are appended automatically
const char* SERVER_URL = "https://your-server.com/cams";
const char* AUTH_TOKEN = "your_secret_token_here";

#endif // AUTH_TOKEN_H
