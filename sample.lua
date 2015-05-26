DoFile("c2lua.cpp");   --xxx
DoFile("c2luaimpl.cpp");

local msg = _G.sendBegin(C2Lua.sizeof("GMM2C_Login"));
local gl = C2Lua.locate(msg, 0, "GMM2C_Login");
gl.data.nDBID = 1100;
local pd = gl.data;
pd.szRoleName = "yyyykkkk";
gl.marketingOpen.nFrom = 111;
_G.LogInfo("dbid = "..pd.nDBID.." name = "..gl.data.szRoleName);