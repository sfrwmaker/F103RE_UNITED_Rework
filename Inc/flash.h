/*
 * flash.h
 *
 *  2023 OCT 19
 *  	Ported from JBC controller source code, tailored to the new hardware
 *
 */

#ifndef _FLASH_H_
#define _FLASH_H_

#include "cfgtypes.h"
#include "ff.h"
#include "vars.h"

typedef enum tip_io_status	{TIP_OK = 0, TIP_IO, TIP_CHECKSUM, TIP_INDEX} TIP_IO_STATUS;
typedef enum active_file  	{W25Q_NOT_MOUNTED = 0, W25Q_NONE, W25Q_TIPS_CURRENT, W25Q_TIPS_BACKUP, W25Q_CONFIG_CURRENT, W25Q_CONFIG_BACKUP, W25Q_CONFIG_TIP_LIST} ACT_FILE;

class W25Q {
	public:
		W25Q(void)			{ }
		FLASH_STATUS	init(void);								// Initialize flash, read tip configuration
		bool			reset();								// Initialize flash, re-check flash size
		bool			mount(void);
		void			umount(void);
		void			close(void);
		bool			loadRecord(RECORD* config_record);
		bool			saveRecord(RECORD* config_record);
		bool			loadPIDparams(PID_PARAMS* pid_params);
		bool			savePIDparams(PID_PARAMS* pid_params);
		TIP_IO_STATUS	loadTipData(TIP* tip, uint8_t tip_index, bool keep = false);
		int16_t 		saveTipData(TIP* tip, bool keep = false); // Return tip index in the file or -1 if error
		bool			formatFlashDrive(void);
		bool			clearTips(void);
		bool			clearConfig(void);
		bool			canDelete(const TCHAR *file_name);
		const TCHAR*	fileName(uint8_t index);
		void			keepMounted(bool keep)					{ keep_mounted = keep; }
		uint8_t			tipListReadNextItem(const char data[], uint8_t size);
		void			tipListEnd(void);
	private:
		TIP_IO_STATUS	returnStatus(bool keep, TIP_IO_STATUS ret_code);
		uint8_t 		TIP_checkSum(TIP* tip, bool write);
		uint8_t			CFG_checkSum(RECORD* cfg, bool write);
		uint8_t			PID_checkSum(PID_PARAMS* pid_params, bool write);
		bool			backup(ACT_FILE type);
		bool			keep_mounted	= false;
		FIL				cfg_f;
		ACT_FILE		act_f = W25Q_NOT_MOUNTED;				// Open file
		const uint16_t	blk_size		= 4096;
		const TCHAR*	fn_tip_calib	= "tipcal.dat";
		const TCHAR*	fn_tip_backup	= "tipcal.bak";
		const TCHAR*	fn_cfg			= "config.dat";
		const TCHAR*	fn_cfg_backup	= "config.bak";
		const TCHAR*	fn_pid			= "pid.dat";
		const TCHAR*	fn_tip_list		= "tip_list.txt";
};

#endif
