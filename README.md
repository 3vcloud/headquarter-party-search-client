**Client**

`cd client`

`build.bat`

sln is created in client/win32build/party_search_client.sln

modify the `client` project, add command arguments for debugging: `-email "<email>" -v -password "<password>" -character "<character_name>" party_search_client`

**Server**

`cd server`

`npm install`

`npm run serve`

Server is now running on localhost
