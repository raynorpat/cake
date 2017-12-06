#include "d3d_local.h"


IDirect3DVertexShader9 *D3D_CreateVertexShader (char *name, char *entrypoint)
{
	ID3DXBuffer *shaderBuffer = NULL;
	ID3DXBuffer *errorBuffer = NULL;
	IDirect3DVertexShader9 *shaderOut = NULL;

	D3DXMACRO defines[] =
	{
		{"VERTEXSHADER", "1"},
		{NULL, NULL}
	};

	char *resbuf = NULL;
	int reslen = FS_LoadFile(name, (void **)&resbuf);
	char *ressrc;
	int result = 0;

	if (!reslen)
		return 0;

	// the file doesn't have a trailing 0, so we need to copy it off
	ressrc = (char *)malloc(reslen + 1);
	memcpy(ressrc, resbuf, reslen);
	ressrc[reslen] = 0;

	// because there are so many places where this can fail, instead of checking HRESULTs here, we
	// just let the caller check if it got a valid shader returned
	D3DXCompileShader(
		ressrc,
		(UINT)strlen(ressrc),
		defines,
		NULL,
		entrypoint,
		"vs_3_0",
		0,
		&shaderBuffer,
		&errorBuffer,
		NULL
	);

	if (shaderBuffer)
	{
		d3d_Device->CreateVertexShader ((DWORD *) shaderBuffer->GetBufferPointer (), &shaderOut);
		shaderBuffer->Release ();
	}

	if (errorBuffer)
	{
		Com_Printf ("Errors compiling %s\n", entrypoint);
		Com_Printf ("%s\n", (char *) errorBuffer->GetBufferPointer ());
		errorBuffer->Release ();
	}

	free(ressrc);
	FS_FreeFile(resbuf);

	return shaderOut;
}


IDirect3DPixelShader9 *D3D_CreatePixelShader (char *name, char *entrypoint)
{
	ID3DXBuffer *shaderBuffer = NULL;
	ID3DXBuffer *errorBuffer = NULL;
	IDirect3DPixelShader9 *shaderOut = NULL;

	D3DXMACRO defines[] =
	{
		{"PIXELSHADER", "1"},
		{NULL, NULL}
	};

	char *resbuf = NULL;
	int reslen = FS_LoadFile(name, (void **)&resbuf);
	char *ressrc;
	int result = 0;

	if (!reslen)
		return 0;

	// the file doesn't have a trailing 0, so we need to copy it off
	ressrc = (char *)malloc(reslen + 1);
	memcpy(ressrc, resbuf, reslen);
	ressrc[reslen] = 0;

	// because there are so many places where this can fail, instead of checking HRESULTs here, we
	// just let the caller check if it got a valid shader returned
	D3DXCompileShader(
		ressrc,
		(UINT)strlen(ressrc),
		defines,
		NULL,
		entrypoint,
		"ps_3_0",
		0,
		&shaderBuffer,
		&errorBuffer,
		NULL
	);

	if (shaderBuffer)
	{
		d3d_Device->CreatePixelShader ((DWORD *) shaderBuffer->GetBufferPointer (), &shaderOut);
		shaderBuffer->Release ();
	}

	if (errorBuffer)
	{
		Com_Printf ("Errors compiling %s\n", entrypoint);
		Com_Printf ("%s\n", (char *) errorBuffer->GetBufferPointer ());
		errorBuffer->Release ();
	}

	free(ressrc);
	FS_FreeFile(resbuf);

	return shaderOut;
}


