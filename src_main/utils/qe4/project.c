#include "qe3.h"
#include "io.h"
#include <Shlobj.h>

int		firstproject;
#define MAX_BSPCOMMANDS 64
char	*bspnames[MAX_BSPCOMMANDS];
char	*bspcommands[MAX_BSPCOMMANDS];
void	GetProjectDirectory(HWND h);
void	GetModDirectory(HWND h);
void	GetAutosaveMap(HWND h);
void	GetEntityFiles(HWND h);
void	GetTextureDirectory(HWND h);
void	GetRemoteBasePath(HWND h);
char	CurrentDirectory[260];

int GetNextFreeBspIndex(void)
{
	int i;

	for(i = 0; i < MAX_BSPCOMMANDS; i++)
	{
		if(!bspnames[i])
			return i;
	}

	return -1;
}

int GetBspIndex(char *text)
{
	int i;

	for(i = 0; i < MAX_BSPCOMMANDS; i++)
	{
		if(bspnames[i] && !strcmp(bspnames[i], text))
			return i;
	}

	return -1;
}

void NewBspCommand(HWND hwndDlg)
{
	char	buf[256], buf2[256];
	HWND	h = GetDlgItem(hwndDlg, IDC_BSPNAME_COMBO);	
	int		i = SendMessage(h, CB_GETCURSEL, 0, 0);
	int		index = GetNextFreeBspIndex();

	if(index == -1)
	{
		MessageBox(g_qeglobals.d_hwndMain, "Cannot add a new BSP menu. Delete an existing one first.", "Error", MB_OK);
		return;
	}

	if(!SendMessage(h, WM_GETTEXT, (WPARAM)255, (LPARAM)buf))
	{
		MessageBox(g_qeglobals.d_hwndMain, "No name to given for the command", "Error", MB_OK);
		return;
	}

	strcpy(buf2, buf);
	if(SendMessage(h, CB_FINDSTRING, (WPARAM)-1, (LPARAM)buf2) != CB_ERR)
	{
		MessageBox(g_qeglobals.d_hwndMain, "A command with that name already exists", buf, MB_OK);
		return;
	}

	i = SendMessage (h, CB_ADDSTRING, 0, (LPARAM)buf);
	if(i != CB_ERR && i != CB_ERRSPACE)
	{
		bspnames[index] = strdup(buf);
		bspcommands[index] = strdup("");
		SendMessage(h, CB_SETCURSEL, (WPARAM)i, 0);
		SendMessage(hwndDlg, WM_COMMAND, (WPARAM)(CBN_SELCHANGE<<16)+IDC_BSPNAME_COMBO, (LPARAM)h);
	}
	else
	{
		MessageBox(g_qeglobals.d_hwndMain, "Error adding string to combobox", "ERROR", MB_OK);
	}
}

void DeleteBspCommand(HWND hwndDlg)
{
	HWND	h = GetDlgItem(hwndDlg, IDC_BSPNAME_COMBO);	
	int		i, i2 = SendMessage(h, CB_GETCURSEL, 0, 0);
	char	buf[256];
	
	SendMessage(h, CB_GETLBTEXT, (WPARAM)i2, (LPARAM)buf);

	i = GetBspIndex(buf);
	if(i == -1)
	{
		MessageBox(g_qeglobals.d_hwndMain, "Could not delete BSP item: Unmatched", "Error", MB_OK);
		return;
	}

	free(bspnames[i]);
	if(bspcommands[i])
		free(bspcommands[i]);
	bspnames[i] = bspcommands[i] = NULL;

	SendMessage(h, CB_DELETESTRING, (WPARAM)i2, 0);
	SendMessage(h, CB_SETCURSEL, 0, 0);
	SendMessage(hwndDlg, WM_COMMAND, (WPARAM)(CBN_SELCHANGE<<16)+IDC_BSPNAME_COMBO, (LPARAM)h);
}

void AcceptBspCommand(HWND hwndDlg)
{
	char	buf[256], buf2[512];
	HWND	h = GetDlgItem(hwndDlg, IDC_BSPNAME_COMBO);	
	int		i = SendMessage(h, CB_GETCURSEL, 0, 0);
	int		index;

	SendMessage(h, CB_GETLBTEXT, (WPARAM)i, (LPARAM)buf);

	index = GetBspIndex(buf);
	SendMessage(GetDlgItem(hwndDlg, IDC_BSPCOMMAND_EDIT), WM_GETTEXT, (WPARAM)511, (LPARAM)buf2);
	free(bspcommands[index]);
	
	bspcommands[index] = strdup(buf2);
}

void SaveSettings(HWND hwndDlg)
{
	FILE	*file;
	char	buf[1024], projdir[512], prev;
	int		i, i2, i3;
	
	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_PROJECTDIRECTORY), buf, 511);

	setdirstr(projdir, buf, "/", NULL);
	sprintf(buf, "%squake.qe4", projdir);
	if(!(file = fopen(buf, "w")))
	{
		MessageBox(hwndDlg, "Could not open config file for writing", "Error", MB_OK);
		return;
	}

	// generate the quake.qe4 configuration file
	fprintf(file, "{\n");

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_PROJECTDIRECTORY), buf, 255);
	fprintf(file, "\"basepath\" \"%s\"\n", buf);

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_MODDIRECTORY), buf, 255);
	if(buf[0])
	{
		fprintf(file, "\"modpath\" \"%s\"\n", buf);
	}

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_REMOTEBASEPATH), buf, 255);
	if(buf[0])
	{
		fprintf(file, "\"remotebasepath\" \"%s\"\n", buf);
	}
	else
	{
		GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_PROJECTDIRECTORY), buf, 255);
		fprintf(file, "\"remotebasepath\" \"%s\"\n", buf);
	}

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_AUTOSAVEMAP), buf, 255);
	fprintf(file, "\"autosave\" \"%s\"\n", buf);

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_ENTITYFILES), buf, 255);
	fprintf(file, "\"entitypath\" \"%s\"\n", buf);

	GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT_TEXTUREDIRECTORY), buf, 512);
	fprintf(file, "\"texturepath\" \"%s\"\n", buf);

	for(i = 0; i < MAX_BSPCOMMANDS; i++)
	{
		if(bspnames[i] && bspcommands[i][0]) // Only write out a BSP line if there is a command for it
		{
			prev = 0;
			for(i2 = i3 = 0; bspcommands[i][i2]; i2++)
			{
				switch(bspcommands[i][i2])
				{
					case ' ':
						if(prev != ' ')
							prev = buf[i3++] = ' ';
						break;
					case '\t':
						if(prev != ' ')
							prev = buf[i3++] = ' ';
						break;
					case 13:
						if(prev != ' ')
							prev = buf[i3++] = ' ';
						else
							prev = 13;
						break;
					case 10:
						if(prev != ' ')
							buf[i3++] = ' ';
						buf[i3++] = '&';
						buf[i3++] = '&';
						buf[i3++] = ' ';
						while(bspcommands[i][i2] != 0 && (bspcommands[i][i2] == ' ' || bspcommands[i][i2] == '\t'))
							i2++;
						prev = 10;
						break;
					default:
						prev = buf[i3++] = bspcommands[i][i2];
						break;
				}
			}
			buf[i3] = 0;
			fprintf(file, "\"bsp_%s\" \"%s\"\n", bspnames[i], buf);
		}
	}

	fprintf(file, "}\n");
	fclose(file);

	if(!firstproject)
		MessageBox(g_qeglobals.d_hwndMain, "New settings will take effect the next time QuakeEd is run.", "Info", MB_OK);
}

BOOL CALLBACK ProjectSettingsDlgProc (
    HWND hwndDlg,	// handle to dialog box
    UINT uMsg,		// message
    WPARAM wParam,	// first message parameter
    LPARAM lParam 	// second message parameter
   )
{
	switch (uMsg)
    {
		case WM_INITDIALOG:
		{
			char	*texturedir;
			char	*basedir;
			char	*moddir;
			char	*autosavemap;
			char	*entitypath;
			char	*remotebasepath;
			char	buf[512], prev, *name, *value;
			epair_t	*ep;
			HWND	h;
			int		index, i, i2;

			for(i = 0; i < MAX_BSPCOMMANDS; i++)
			{
				bspnames[i] = NULL;
				bspcommands[i] = NULL;
			}

			GetCurrentDirectory(259, CurrentDirectory);
			h = GetDlgItem(hwndDlg, IDC_BSPNAME_COMBO);
			SendMessage(h, CB_LIMITTEXT, (WPARAM)255, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_BSPCOMMAND_EDIT), EM_LIMITTEXT, (WPARAM)511, 0);

			if(!firstproject)
			{
				texturedir =		ValueForKey(g_qeglobals.d_project_entity, "texturepath");
				basedir =			ValueForKey(g_qeglobals.d_project_entity, "basepath");
				moddir =			ValueForKey(g_qeglobals.d_project_entity, "modpath");
				autosavemap =		ValueForKey(g_qeglobals.d_project_entity, "autosave");
				entitypath =		ValueForKey(g_qeglobals.d_project_entity, "entitypath");
				remotebasepath =	ValueForKey(g_qeglobals.d_project_entity, "remotebasepath");

				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_TEXTUREDIRECTORY), WM_SETTEXT, 0, (LPARAM)texturedir);
				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_PROJECTDIRECTORY), WM_SETTEXT, 0, (LPARAM)basedir);
				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_MODDIRECTORY), WM_SETTEXT, 0, (LPARAM)moddir);
				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_AUTOSAVEMAP), WM_SETTEXT, 0, (LPARAM)autosavemap);
				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_ENTITYFILES), WM_SETTEXT, 0, (LPARAM)entitypath);				
				SendMessage (GetDlgItem(hwndDlg, IDC_EDIT_REMOTEBASEPATH), WM_SETTEXT, 0, (LPARAM)remotebasepath);
				
				for (ep = g_qeglobals.d_project_entity->epairs ; ep ; ep=ep->next)
				{
					if (ep->key[0] == 'b' && ep->key[1] == 's' && ep->key[2] == 'p' && ep->key[3] == '_')
					{
						name = ep->key+4;
						value = ep->value;
						SendMessage (h, CB_ADDSTRING, 0, (LPARAM)name);
						index = GetNextFreeBspIndex();
						prev = 0;
						for(i = i2 = 0; value[i]; i++)
						{
							switch(value[i])
							{
							case ' ':
								if(prev != ' ' && prev != '&')
									prev = buf[i2++] = ' ';
								break;
							case '\t':
								if(prev != ' ')
									prev = buf[i2++] = ' ';
								break;
							case '&':
								if(prev != '&' && prev != ' ')
								{
									buf[i2++] = ' ';
								}
								if(prev == '&')
								{
									buf[i2++] = 13;
									buf[i2++] = 10;
									while(value[i] != 0 && (value[i] == ' ' || value[i] == '\t'))
										i++;
								}
								prev = '&';
								break;
							default:
								prev = buf[i2++] = value[i];
								break;
							}
						}
						buf[i2] = 0;
						bspnames[index] = strdup(name);
						bspcommands[index] = strdup(buf);
					}
				}
				SendMessage (h, CB_SETCURSEL, 0, 0);
				SendMessage(hwndDlg, WM_COMMAND, (WPARAM)(CBN_SELCHANGE<<16)+IDC_BSPNAME_COMBO, (LPARAM)h);
			}

			return FALSE;
			break;
		}
		
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					SetCurrentDirectory(CurrentDirectory);
					if(!SendMessage(GetDlgItem(hwndDlg, IDC_EDIT_PROJECTDIRECTORY), EM_LINELENGTH, 0, 0))
					{
						MessageBox(g_qeglobals.d_hwndMain, "No project directory specified", "Error", MB_OK);
						return FALSE;
					}
					else if(!SendMessage(GetDlgItem(hwndDlg, IDC_EDIT_AUTOSAVEMAP), EM_LINELENGTH, 0, 0))
					{
						MessageBox(g_qeglobals.d_hwndMain, "No auto-save map specified", "Error", MB_OK);
						return FALSE;
					}
					else if(!SendMessage(GetDlgItem(hwndDlg, IDC_EDIT_ENTITYFILES), EM_LINELENGTH, 0, 0))
					{
						MessageBox(g_qeglobals.d_hwndMain, "No entity definition files specified", "Error", MB_OK);
						return FALSE;
					}
					else if(!SendMessage(GetDlgItem(hwndDlg, IDC_EDIT_TEXTUREDIRECTORY), EM_LINELENGTH, 0, 0))
					{
						MessageBox(g_qeglobals.d_hwndMain, "No texture directory specified", "Error", MB_OK);
						return FALSE;
					}
					else
					{
						SaveSettings(hwndDlg);
						EndDialog(hwndDlg, 1);
					}
				}
				break;
				
				case IDCANCEL:
				{
					int	i;
					for(i = 0; i < MAX_BSPCOMMANDS; i++)
					{
						if(bspcommands[i])
							free(bspcommands[i]);
						if(bspnames[i])
							free(bspnames[i]);
					}
					if(!firstproject)
						EndDialog(hwndDlg, 0);
					else
						if(MessageBox(g_qeglobals.d_hwndMain, "QuakeEd cannot run without a default project.\nPress OK to exit, or CANCEL to keep editing the project.", "info", MB_OKCANCEL /* MB_OK */ ) == IDOK)
							exit(0);
				}
				break;
				
				case IDC_BUTTON_PROJECTDIRECTORY:
				case IDC_BUTTON_MODDIRECTORY:
				case IDC_BUTTON_AUTOSAVEMAP:
				case IDC_BUTTON_ENTITYFILES:
				case IDC_BUTTON_TEXTUREDIRECTORY:
				case IDC_BUTTON_REMOTEBASEPATH:
				case IDC_BSPNEW_BUTTON:
				case IDC_BSPDELETE_BUTTON:
				case IDC_BSPACCEPT_BUTTON:
				{
					switch(HIWORD(wParam))
					{
						case BN_CLICKED:
						{
							int		ibutton = LOWORD(wParam);
							
							switch(ibutton)
							{
								case IDC_BUTTON_PROJECTDIRECTORY:
									GetProjectDirectory(hwndDlg);
									break;
								case IDC_BUTTON_MODDIRECTORY:
									GetModDirectory(hwndDlg);
									break;
								case IDC_BUTTON_AUTOSAVEMAP:
									GetAutosaveMap(hwndDlg);
									break;
								case IDC_BUTTON_ENTITYFILES:
									GetEntityFiles(hwndDlg);
									break;
								case IDC_BUTTON_TEXTUREDIRECTORY:
									GetTextureDirectory(hwndDlg);
									break;
								case IDC_BUTTON_REMOTEBASEPATH:
									GetRemoteBasePath(hwndDlg);
									break;
								case IDC_BSPNEW_BUTTON:
									NewBspCommand(hwndDlg);
									break;
								case IDC_BSPDELETE_BUTTON:
									DeleteBspCommand(hwndDlg);
									break;
								case IDC_BSPACCEPT_BUTTON:
									AcceptBspCommand(hwndDlg);
									break;
							}
							break;
						}
					}
					break;
				}

				case IDC_BSPNAME_COMBO:
				{
					switch(HIWORD(wParam))
					{
						case CBN_SELCHANGE:
						{
							char buf[256];
							int index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

							SendMessage((HWND)lParam, CB_GETLBTEXT, (WPARAM)index, (LPARAM)buf);
							index = GetBspIndex(buf);
							if(index == -1)
							{
//								MessageBox(g_qeglobals.d_hwndMain, buf, "Error: Unmatched", MB_OK);
								return FALSE;
							}
							SendMessage(GetDlgItem(hwndDlg, IDC_BSPCOMMAND_EDIT), WM_SETTEXT, 0, (LPARAM)bspcommands[index]);
						}
						break;
					}
				}
				break;
			}
		}

		default:
			return FALSE;
			break;
	}
}

static OPENFILENAME ofn;       /* common dialog box structure   */
static char szDirName[MAX_PATH];    /* directory string              */
static char szFile[260];       /* filename string               */
static char szFileTitle[260];  /* file title string             */
static char szFilter[260];     /* filter string                 */
static char szProjectFilter[260] =     /* filter string                 */
	"QuakeEd project (*.qe4)\0*.qe4\0\0";
static char chReplace;         /* string separator for szFilter */
static int i, cbString;        /* integer count variables       */
static HANDLE hf;              /* file handle                   */

/*
DlgDirList() sucks (no LFN support), so this will take its place.
*/
void MyDlgDirList(HWND hwndDlg, char *szDirName, int listbox, int statictext, int unused)
{
	char				work[MAX_PATH];
	struct _finddata_t	fileinfo;
	int					handle, h;
	HWND				listhWnd = GetDlgItem(hwndDlg, listbox);
	HWND				statichWnd = GetDlgItem(hwndDlg, statictext);

	if(!_fullpath(work, szDirName, MAX_PATH))
	{
		GetCurrentDirectory(MAX_PATH-1, work);
	}

	strcpy(szDirName, work);
	SendMessage(statichWnd, WM_SETTEXT, (WPARAM) 0, (LPARAM)szDirName);
	SendMessage(listhWnd, LB_RESETCONTENT, 0, 0);
	
	if(szDirName[strlen(szDirName)-1] != '\\')
		sprintf(work, "%s\\*", szDirName);
	else
		sprintf(work, "%s*", szDirName);

	for(h = handle = _findfirst(work, &fileinfo); h != -1; h = _findnext(handle, &fileinfo))
	{
		if (!(fileinfo.attrib & _A_SUBDIR))
			continue;
		if (!strcmp(fileinfo.name, "."))
			continue;

		SendMessage(listhWnd, LB_ADDSTRING, 0 , (LPARAM)fileinfo.name);
	}
}

/*
replaces DlgDirSelectEx()
*/
int MyDlgDirSelectEx(HWND hwndDlg, char *szFile, int size, int listbox)
{
	HWND	listhWnd = GetDlgItem(hwndDlg, listbox);
	int		index;

	index = SendMessage(listhWnd, LB_GETCURSEL, 0, 0);

	if(size < SendMessage(listhWnd, LB_GETTEXTLEN, (WPARAM)index, 0))
		return FALSE;

	SendMessage(listhWnd, LB_GETTEXT, (WPARAM)index, (LPARAM)szFile);
	return TRUE;
}

BOOL CALLBACK SelectDirDlgProc (
    HWND hwndDlg,	// handle to dialog box
    UINT uMsg,		// message
    WPARAM wParam,	// first message parameter
    LPARAM lParam 	// second message parameter
   )
{
	switch (uMsg)
    {
		case WM_INITDIALOG:
		{
			MyDlgDirList(hwndDlg, szDirName, IDC_SELECTDIR_LIST, IDC_SELECTDIR_STATIC, DDL_DIRECTORY|DDL_EXCLUSIVE);
			DlgDirListComboBox(hwndDlg, szDirName, IDC_SELECTDIR_COMBO, 0, DDL_DRIVES);
			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					SendMessage(GetDlgItem(hwndDlg, IDC_SELECTDIR_STATIC), WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName);

					if(szDirName[strlen(szDirName)-1] != '\\')
						strcat(szDirName, "\\");

					EndDialog(hwndDlg, 1);
					break;
				}

				case IDCANCEL:
				{
					EndDialog(hwndDlg, 0);
					break;
				}

				case IDC_SELECTDIR_LIST:
				{
					switch(HIWORD(wParam))
					{
						case LBN_DBLCLK:
						{
							if(MyDlgDirSelectEx(hwndDlg, szFile, MAX_PATH-1, IDC_SELECTDIR_LIST))
							{
								SendMessage(GetDlgItem(hwndDlg, IDC_SELECTDIR_STATIC), WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName);
								if(szDirName[strlen(szDirName)-1] != '\\')
								{
									strcat(szDirName, "\\");
									strcat(szDirName, szFile);
								}
								else
									strcat(szDirName, szFile);

								MyDlgDirList(hwndDlg, szDirName, IDC_SELECTDIR_LIST, IDC_SELECTDIR_STATIC, DDL_DIRECTORY|DDL_EXCLUSIVE);
							}
							break;
						}
					}
					break;
				}

				case IDC_SELECTDIR_COMBO:
				{
					switch(HIWORD(wParam))
					{
						case CBN_SELCHANGE:
						{
							int		index = SendMessage(GetDlgItem(hwndDlg, IDC_SELECTDIR_COMBO), CB_GETCURSEL, 0, 0);

							if(DlgDirSelectComboBoxEx(hwndDlg, szDirName, 5, IDC_SELECTDIR_COMBO))
								MyDlgDirList(hwndDlg, szDirName, IDC_SELECTDIR_LIST, IDC_SELECTDIR_STATIC, DDL_DIRECTORY|DDL_EXCLUSIVE);

							break;
						}
					}
					break;
				}

			}
			break;
		}

		default:
		{
			return FALSE;
			break;
		}
	}
	return FALSE;
}

int SelectDir(HWND edithwnd)
{
	BROWSEINFO bi = { 0 };
	LPITEMIDLIST pidl;
	
	bi.lpszTitle = "Pick a Directory";
	pidl = SHBrowseForFolder (&bi);
	
	if(pidl != 0)
	{
		char DirName[MAX_PATH];

		if (SHGetPathFromIDList(pidl, DirName))
		{
			setstr(szDirName, DirName, "\\", NULL, NULL);
			SendMessage(edithwnd, WM_SETTEXT, 0, (LPARAM)szDirName);
		}
		
		GlobalFree(pidl);
		return true;
	}
	return false;
}

void DoProject(int first)
{
	firstproject = first;
	DialogBox(g_qeglobals.d_hInstance, (char *)IDD_PROJECTDIALOG, g_qeglobals.d_hwndMain, ProjectSettingsDlgProc);
}

void GetProjectDirectory(HWND h)
{
	HWND	edithwnd = GetDlgItem(h, IDC_EDIT_PROJECTDIRECTORY);

	if(!SendMessage(edithwnd, WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName))
		strcpy(szDirName, CurrentDirectory);

	SelectDir(edithwnd);
}

void GetModDirectory(HWND h)
{
	HWND	edithwnd = GetDlgItem(h, IDC_EDIT_MODDIRECTORY);

	if(!SendMessage(edithwnd, WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName))
		strcpy(szDirName, CurrentDirectory);

	SelectDir(edithwnd);
}

void GetAutosaveMap(HWND h)
{
	sprintf(szDirName, "%smaps", CurrentDirectory);
	szFile[0] = 0;
	GetCurrentDirectory(MAX_PATH-1, szDirName);
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = h;
	ofn.lpstrFilter = "QuakeEd file (*.map)\0*.map\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = sizeof(szFileTitle);
	ofn.lpstrInitialDir = szDirName;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrTitle = "Select Auto-Save Map File";
	if(GetSaveFileName(&ofn))
	{
		SendMessage (GetDlgItem(h, IDC_EDIT_AUTOSAVEMAP), WM_SETTEXT, 0, (LPARAM)szFile);
	}
}

void GetEntityFiles(HWND h)
{
	szFile[0] = 0;
	MessageBox(g_qeglobals.d_hwndMain, "If you have a single entity definition file, open it.\nIf your entity definitions are in multiple files,\nopen any file in their directory and then replace\nthe filename with a wildcard expression (EG: *.c or\n*.qc)", "info", MB_OK);
	GetCurrentDirectory(MAX_PATH-1, szDirName);
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = h;
	ofn.lpstrFilter = "QuakeEd entity definition file (*.def)\0*.def\0QuakeEd entity definition file (*.c)\0*.c\0QuakeEd entity definition file (*.qc)\0*.qc\0All files\0*.*\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = sizeof(szFileTitle);
	ofn.lpstrInitialDir = CurrentDirectory;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrTitle = "Select Entity Definition File";
	if(GetOpenFileName(&ofn))
	{
		SendMessage (GetDlgItem(h, IDC_EDIT_ENTITYFILES), WM_SETTEXT, 0, (LPARAM)szFile);
	}
}

void GetRemoteBasePath(HWND h)
{
	HWND	edithwnd = GetDlgItem(h, IDC_EDIT_REMOTEBASEPATH);

	if(!SendMessage(edithwnd, WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName))
		strcpy(szDirName, CurrentDirectory);

	SelectDir(edithwnd);
}

void GetTextureDirectory(HWND h)
{
	HWND	edithwnd = GetDlgItem(h, IDC_EDIT_TEXTUREDIRECTORY);

	if(!SendMessage(edithwnd, WM_GETTEXT, (WPARAM)MAX_PATH-1, (LPARAM)szDirName))
		strcpy(szDirName, CurrentDirectory);

	SelectDir(edithwnd);
}
