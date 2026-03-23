// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

void listInstalledGames()
{
	Result rc = 0;
	s32 out_entrycount = 0;
	NsApplicationRecord *records = NULL;

	rc = nsInitialize();
	if (R_FAILED(rc))
	{
		printf("Failed to init ns service: 0x%x\n", rc);
		return;
	}

	rc = ncmInitialize();
	if (R_FAILED(rc))
	{
		printf("Failed to init ncm service: 0x%x\n", rc);
		nsExit();
		return;
	}

	records = calloc(100, sizeof(NsApplicationRecord));
	if (records == NULL)
	{
		nsExit();
		return;
	}

	rc = nsListApplicationRecord(records, 100, 0, &out_entrycount);
	if (R_FAILED(rc))
	{
		printf("Error listing records: 0x%x\n", rc);
		free(records);
		nsExit();
		return;
	}

	printf("Found %d installed games:\n", out_entrycount);
	printf("----------------------------\n");

	u64 totalSize = 0;
	int shown = 0;
	for (int i = 0; i < out_entrycount; i++)
	{
		NsApplicationControlData controlData;
		u64 actual_size = 0;
		char titleName[0x201];
		memset(titleName, 0, sizeof(titleName));

		rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, records[i].application_id, &controlData, sizeof(NsApplicationControlData), &actual_size);
		if (R_SUCCEEDED(rc))
		{
			for (int lang = 0; lang < 16; lang++)
			{
				if (controlData.nacp.lang[lang].name[0] != '\0')
				{
					strncpy(titleName, controlData.nacp.lang[lang].name, sizeof(titleName) - 1);
					break;
				}
			}
		}

		if (titleName[0] == '\0')
			continue;

		u64 gameSizeBytes = 0;
		NcmStorageId storageIds[] = { NcmStorageId_SdCard, NcmStorageId_BuiltInUser };
		for (int s = 0; s < 2; s++)
		{
			NcmContentStorage cs;
			NcmContentMetaDatabase db;

			rc = ncmOpenContentStorage(&cs, storageIds[s]);
			if (R_FAILED(rc))
				continue;

			rc = ncmOpenContentMetaDatabase(&db, storageIds[s]);
			if (R_FAILED(rc))
			{
				ncmContentStorageClose(&cs);
				continue;
			}

			s32 metaTotal = 0;
			s32 metaWritten = 0;
			NcmContentMetaKey metaKeys[16];
			rc = ncmContentMetaDatabaseList(&db, &metaTotal, &metaWritten, metaKeys, 16, NcmContentMetaType_Application, records[i].application_id, records[i].application_id, records[i].application_id, NcmContentInstallType_Full);
			if (R_SUCCEEDED(rc) && metaWritten > 0)
			{
				for (int m = 0; m < metaWritten; m++)
				{
					NcmContentInfo contentInfos[32];
					s32 contentCount = 0;
					rc = ncmContentMetaDatabaseListContentInfo(&db, &contentCount, contentInfos, 32, &metaKeys[m], 0);
					if (R_SUCCEEDED(rc))
					{
						for (int c = 0; c < contentCount; c++)
						{
							s64 contentSize = 0;
							rc = ncmContentStorageGetSizeFromContentId(&cs, &contentSize, &contentInfos[c].content_id);
							if (R_SUCCEEDED(rc))
								gameSizeBytes += (u64)contentSize;
						}
					}
				}
			}

			ncmContentMetaDatabaseClose(&db);
			ncmContentStorageClose(&cs);

			if (gameSizeBytes > 0)
				break;
		}

		totalSize += gameSizeBytes;
		shown++;

		double gameSizeMB = gameSizeBytes / (1024.0 * 1024.0);
		if (gameSizeMB >= 1024.0)
			printf("[%02d] %s (%.2f GB)\n", shown, titleName, gameSizeMB / 1024.0);
		else
			printf("[%02d] %s (%.2f MB)\n", shown, titleName, gameSizeMB);
	}

	printf("----------------------------\n");
	double totalMB = totalSize / (1024.0 * 1024.0);
	if (totalMB >= 1024.0)
		printf("Total: %d games, %.2f GB\n", shown, totalMB / 1024.0);
	else
		printf("Total: %d games, %.2f MB\n", shown, totalMB);

	free(records);
	ncmExit();
	nsExit();
}

// Main program entrypoint
int main(int argc, char* argv[])
{
	// This example uses a text console, as a simple way to output text to the screen.
	// If you want to write a software-rendered graphics application,
	//   take a look at the graphics/simplegfx example, which uses the libnx Framebuffer API instead.
	// If on the other hand you want to write an OpenGL based application,
	//   take a look at the graphics/opengl set of examples, which uses EGL instead.
	consoleInit(NULL);

	// Configure our supported input layout: a single player with standard controller styles
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);

	// Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
	PadState pad;
	padInitializeDefault(&pad);

	// Other initialization goes here. As a demonstration, we print hello world.
	printf("GameLister\n");
	printf("Press A to list games.\nPress Plus to exit.\n");

	// Main loop
	while (appletMainLoop())
	{
		// Scan the gamepad. This should be done once for each frame
		padUpdate(&pad);

		// padGetButtonsDown returns the set of buttons that have been
		// newly pressed in this frame compared to the previous one
		u64 kDown = padGetButtonsDown(&pad);
		if (kDown & HidNpadButton_Plus)
			break; // break in order to return to hbmenu
		if (kDown & HidNpadButton_A)
		{
			printf("\n");
			listInstalledGames();
		}

		// Update the console, sending a new frame to the display
		consoleUpdate(NULL);
	}

	// Deinitialize and clean up resources used by the console (important!)
	consoleExit(NULL);
	return 0;
}