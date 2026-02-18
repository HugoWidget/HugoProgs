#pragma once
#include <Windows.h>


DWORD DuplicateWinloginToken(DWORD dwSessionId, DWORD dwDesiredAccess, PHANDLE phToken);
DWORD CreateUIAccessToken(PHANDLE phToken);
BOOL CheckForUIAccess(DWORD* pdwErr, DWORD* pfUIAccess);
DWORD PrepareForUIAccess();

