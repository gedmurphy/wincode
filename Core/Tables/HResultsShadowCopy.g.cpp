/*!

    @file Core/Tables/HResultsShadowCopy.g.cpp

    @brief Volume Shadow Copy HRESULTs, generated from vsserror.h.

    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.

             Generated from Windows SDK 10.0.28000.0. Sorted by value, with
             the primary name for each value first, so a lookup can
             binary search to it and walk forward over the aliases.

    @copyright Copyright (c) 2026 Ged Murphy

    This file is part of WinCode.

    WinCode is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    WinCode is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with WinCode. If not, see <https://www.gnu.org/licenses/>.

*/

#include "Tables.hpp"


static const TableEntry g_Entries[] =
{
    { 0x00042309u, 0, "VSS_S_ASYNC_PENDING", "The asynchronous operation is pending." },
    { 0x0004230Au, 0, "VSS_S_ASYNC_FINISHED", "The asynchronous operation has completed." },
    { 0x0004230Bu, 0, "VSS_S_ASYNC_CANCELLED", "The asynchronous operation has been cancelled." },
    { 0x00042321u, 0, "VSS_S_SOME_SNAPSHOTS_NOT_IMPORTED", "Some shadow copies were not successfully imported." },
    { 0x80042301u, 0, "VSS_E_BAD_STATE", "A function call was made when the object was in an incorrect state for that function" },
    { 0x80042302u, 0, "VSS_E_UNEXPECTED", "A Volume Shadow Copy Service component encountered an unexpected error. Check the Application event log for more information." },
    { 0x80042303u, 0, "VSS_E_PROVIDER_ALREADY_REGISTERED", "The provider has already been registered." },
    { 0x80042304u, 0, "VSS_E_PROVIDER_NOT_REGISTERED", "The volume shadow copy provider is not registered in the system." },
    { 0x80042306u, 0, "VSS_E_PROVIDER_VETO", "The shadow copy provider had an error. Check the System and Application event logs for more information." },
    { 0x80042307u, 0, "VSS_E_PROVIDER_IN_USE", "The shadow copy provider is currently in use and cannot be unregistered." },
    { 0x80042308u, 0, "VSS_E_OBJECT_NOT_FOUND", "The specified object was not found." },
    { 0x8004230Cu, 0, "VSS_E_VOLUME_NOT_SUPPORTED", "Shadow copying the specified volume is not supported." },
    { 0x8004230Du, 0, "VSS_E_OBJECT_ALREADY_EXISTS", "The object already exists." },
    { 0x8004230Eu, 0, "VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER", "The given shadow copy provider does not support shadow copying the specified volume." },
    { 0x8004230Fu, 0, "VSS_E_UNEXPECTED_PROVIDER_ERROR", "The shadow copy provider had an unexpected error while trying to process the specified operation." },
    { 0x80042310u, 0, "VSS_E_CORRUPT_XML_DOCUMENT", "The given XML document is invalid.  It is either incorrectly-formed XML or it does not match the schema.  This error code is deprecated." },
    { 0x80042311u, 0, "VSS_E_INVALID_XML_DOCUMENT", "The given XML document is invalid.  It is either incorrectly-formed XML or it does not match the schema." },
    { 0x80042312u, 0, "VSS_E_MAXIMUM_NUMBER_OF_VOLUMES_REACHED", "The maximum number of volumes for this operation has been reached." },
    { 0x80042313u, 0, "VSS_E_FLUSH_WRITES_TIMEOUT", "The shadow copy provider timed out while flushing data to the volume being shadow copied. This is probably due to excessive activity on the volume. Try again later when the volume is not being used so heavily." },
    { 0x80042314u, 0, "VSS_E_HOLD_WRITES_TIMEOUT", "The shadow copy provider timed out while holding writes to the volume being shadow copied. This is probably due to excessive activity on the volume by an application or a system service. Try again later when activity on the volume is reduced." },
    { 0x80042315u, 0, "VSS_E_UNEXPECTED_WRITER_ERROR", "VSS encountered problems while sending events to writers." },
    { 0x80042316u, 0, "VSS_E_SNAPSHOT_SET_IN_PROGRESS", "Another shadow copy creation is already in progress. Wait a few moments and try again." },
    { 0x80042317u, 0, "VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED", "The specified volume has already reached its maximum number of shadow copies." },
    { 0x80042318u, 0, "VSS_E_WRITER_INFRASTRUCTURE", "An error was detected in the Volume Shadow Copy Service (VSS). The problem occurred while trying to contact VSS writers. Verify that the Event System service and the VSS service are running and check for associated errors in the event logs." },
    { 0x80042319u, 0, "VSS_E_WRITER_NOT_RESPONDING", "A writer did not respond to a GatherWriterStatus call.  The writer may either have terminated or it may be stuck.  Check the System and Application event logs for more information." },
    { 0x8004231Au, 0, "VSS_E_WRITER_ALREADY_SUBSCRIBED", "The writer has already successfully called the Subscribe function.  It cannot call Subscribe multiple times." },
    { 0x8004231Bu, 0, "VSS_E_UNSUPPORTED_CONTEXT", "The shadow copy provider does not support the specified shadow copy type." },
    { 0x8004231Du, 0, "VSS_E_VOLUME_IN_USE", "The specified shadow copy storage association is in use and so can't be deleted." },
    { 0x8004231Eu, 0, "VSS_E_MAXIMUM_DIFFAREA_ASSOCIATIONS_REACHED", "Maximum number of shadow copy storage associations already reached." },
    { 0x8004231Fu, 0, "VSS_E_INSUFFICIENT_STORAGE", "Insufficient storage available to create either the shadow copy storage file or other shadow copy data." },
    { 0x80042320u, 0, "VSS_E_NO_SNAPSHOTS_IMPORTED", "No shadow copies were successfully imported." },
    { 0x80042321u, 0, "VSS_E_SOME_SNAPSHOTS_NOT_IMPORTED", "Some shadow copies were not successfully imported." },
    { 0x80042322u, 0, "VSS_E_MAXIMUM_NUMBER_OF_REMOTE_MACHINES_REACHED", "The maximum number of remote machines for this operation has been reached." },
    { 0x80042323u, 0, "VSS_E_REMOTE_SERVER_UNAVAILABLE", "The remote server is unavailable." },
    { 0x80042324u, 0, "VSS_E_REMOTE_SERVER_UNSUPPORTED", "The remote server is running a version of the Volume Shadow Copy Service that does not support remote shadow-copy creation." },
    { 0x80042325u, 0, "VSS_E_REVERT_IN_PROGRESS", "A revert is currently in progress for the specified volume.  Another revert cannot be initiated until the current revert completes." },
    { 0x80042326u, 0, "VSS_E_REVERT_VOLUME_LOST", "The volume being reverted was lost during revert." },
    { 0x80042327u, 0, "VSS_E_REBOOT_REQUIRED", "A reboot is required after completing this operation." },
    { 0x80042328u, 0, "VSS_E_TRANSACTION_FREEZE_TIMEOUT", "A timeout occurred while freezing a transaction manager." },
    { 0x80042329u, 0, "VSS_E_TRANSACTION_THAW_TIMEOUT", "Too much time elapsed between freezing a transaction manager and thawing the transaction manager." },
    { 0x8004232Au, 0, "VSS_E_UNSELECTED_VOLUME", "The requested operation would overwrite a volume that is not explicitly selected. For more information, check the Application event log." },
    { 0x8004232Bu, 0, "VSS_E_SNAPSHOT_NOT_IN_SET", "The shadow copy ID was not found in the backup components document for the shadow copy set." },
    { 0x8004232Cu, 0, "VSS_E_NESTED_VOLUME_LIMIT", "The specified volume is nested too deeply to participate in the VSS operation." },
    { 0x8004232Du, 0, "VSS_E_VOLUME_NOT_LOCAL", "The volume being backed up is not mounted on the local host." },
    { 0x8004232Eu, 0, "VSS_E_CLUSTER_TIMEOUT", "A timeout occurred while preparing a cluster shared volume for backup." },
    { 0x8004232Fu, 0, "VSS_E_NOT_SUPPORTED", "The requested operation is not supported." },
    { 0x80042336u, 0, "VSS_E_WRITERERROR_PARTIAL_FAILURE", "The writer experienced a partial failure. Check the component level error state for more information." },
    { 0x800423F0u, 0, "VSS_E_WRITERERROR_INCONSISTENTSNAPSHOT", "The shadow-copy set only contains only a subset of the volumes needed to correctly backup the selected components of the writer." },
    { 0x800423F1u, 0, "VSS_E_WRITERERROR_OUTOFRESOURCES", "A resource allocation failed while processing this operation." },
    { 0x800423F2u, 0, "VSS_E_WRITERERROR_TIMEOUT", "The writer's timeout expired between the Freeze and Thaw events." },
    { 0x800423F3u, 0, "VSS_E_WRITERERROR_RETRYABLE", "The writer experienced a transient error.  If the backup process is retried, the error may not reoccur." },
    { 0x800423F4u, 0, "VSS_E_WRITERERROR_NONRETRYABLE", "The writer experienced a non-transient error.  If the backup process is retried, the error is likely to reoccur." },
    { 0x800423F5u, 0, "VSS_E_WRITERERROR_RECOVERY_FAILED", "The writer experienced an error while trying to recover the shadow-copy volume." },
    { 0x800423F6u, 0, "VSS_E_BREAK_REVERT_ID_FAILED", "The shadow copy set break operation failed because the disk/partition identities could not be reverted. The target identity already exists on the machine or cluster and must be masked before this operation can succeed." },
    { 0x800423F7u, 0, "VSS_E_LEGACY_PROVIDER", "This version of the hardware provider does not support this operation." },
    { 0x800423F8u, 0, "VSS_E_MISSING_DISK", "An expected disk did not arrive in the system." },
    { 0x800423F9u, 0, "VSS_E_MISSING_HIDDEN_VOLUME", "An expected hidden volume did not arrive in the system. Check the Application event log for more information." },
    { 0x800423FAu, 0, "VSS_E_MISSING_VOLUME", "An expected volume did not arrive in the system. Check the Application event log for more information." },
    { 0x800423FBu, 0, "VSS_E_AUTORECOVERY_FAILED", "The autorecovery operation failed to complete on the shadow copy." },
    { 0x800423FCu, 0, "VSS_E_DYNAMIC_DISK_ERROR", "An error occurred in processing the dynamic disks involved in the operation." },
    { 0x800423FDu, 0, "VSS_E_NONTRANSPORTABLE_BCD", "The given Backup Components Document is for a non-transportable shadow copy. This operation can only be done on transportable shadow copies." },
    { 0x800423FEu, 0, "VSS_E_CANNOT_REVERT_DISKID", "The MBR signature or GPT ID for one or more disks could not be set to the intended value. Check the Application event log for more information." },
    { 0x800423FFu, 0, "VSS_E_RESYNC_IN_PROGRESS", "The LUN resynchronization operation could not be started because another resynchronization operation is already in progress." },
    { 0x80042400u, 0, "VSS_E_CLUSTER_ERROR", "The clustered disks could not be enumerated or could not be put into cluster maintenance mode. Check the System event log for cluster related events and the Application event log for VSS related events." },
    { 0x80042401u, 0, "VSS_E_ASRERROR_DISK_ASSIGNMENT_FAILED", "There are too few disks on this computer or one or more of the disks is too small. Add or change disks so they match the disks in the backup, and try the restore again." },
    { 0x80042402u, 0, "VSS_E_ASRERROR_DISK_RECREATION_FAILED", "Windows cannot create a disk on this computer needed to restore from the backup. Make sure the disks are properly connected, or add or change disks, and try the restore again." },
    { 0x80042403u, 0, "VSS_E_ASRERROR_NO_ARCPATH", "The computer needs to be restarted to finish preparing a hard disk for restore. To continue, restart your computer and run the restore again." },
    { 0x80042404u, 0, "VSS_E_ASRERROR_MISSING_DYNDISK", "The backup failed due to a missing disk for a dynamic volume. Ensure the disk is online and retry the backup." },
    { 0x80042405u, 0, "VSS_E_ASRERROR_SHARED_CRIDISK", "Automated System Recovery failed the shadow copy, because a selected critical volume is located on a cluster shared disk. This is an unsupported configuration." },
    { 0x80042406u, 0, "VSS_E_ASRERROR_DATADISK_RDISK0", "A data disk is currently set as active in BIOS. Set some other disk as active or use the DiskPart utility to clean the data disk, and then retry the restore operation." },
    { 0x80042407u, 0, "VSS_E_ASRERROR_RDISK0_TOOSMALL", "The disk that is set as active in BIOS is too small to recover the original system disk. Replace the disk with a larger one and retry the restore operation." },
    { 0x80042408u, 0, "VSS_E_ASRERROR_CRITICAL_DISKS_TOO_SMALL", "Failed to find enough suitable disks for recreating all critical disks. The number of available disks should be same or greater than the number of critical disks at time of backup, and each one of the disks must be of same or greater size." },
    { 0x80042409u, 0, "VSS_E_WRITER_STATUS_NOT_AVAILABLE", "Writer status is not available for one or more writers.  A writer may have reached the limit to the number of available backup-restore session states." },
    { 0x8004240Au, 0, "VSS_E_ASRERROR_DYNAMIC_VHD_NOT_SUPPORTED", "A critical dynamic disk is a Virtual Hard Disk (VHD). This is an unsupported configuration. Check the Application event log for more details." },
    { 0x80042411u, 0, "VSS_E_CRITICAL_VOLUME_ON_INVALID_DISK", "A critical volume selected for backup exists on a disk which cannot be backed up by ASR." },
    { 0x80042412u, 0, "VSS_E_ASRERROR_RDISK_FOR_SYSTEM_DISK_NOT_FOUND", "No disk that can be used for recovering the system disk can be found. Try the following: 1) A probable system disk may have been excluded by mistake. a.  Review the list of disks that you have excluded from the recovery for a likely disk. b.  Type LIST DISK command in the DISKPART command interpreter. The probable system disk is usually the first disk listed in the results. c.  If possible, remove the disk from the exclusion list and then retry the recovery. 2) A USB disk may have been assigned as a system disk. a.  Detach all USB disks from the computer. b.  Reboot into Windows Recovery Environment (Win RE), then reattach USB disks and retry the recovery. 3) An invalid disk may have been assigned as system disk. a.  Physically detach the disk from your computer. Then boot into Win RE to retry the recovery." },
    { 0x80042413u, 0, "VSS_E_ASRERROR_NO_PHYSICAL_DISK_AVAILABLE", "Windows did not find any fixed disk that can be used to recreate volumes present in backup. Ensure disks are online, and disk drivers are installed to access the disk(s). 'diskpart.exe' tool with list disks command can be used to see the list of available fixed disks on the system." },
    { 0x80042414u, 0, "VSS_E_ASRERROR_FIXED_PHYSICAL_DISK_AVAILABLE_AFTER_DISK_EXCLUSION", "Windows did not find any disk which it can use for recreating volumes present in backup. Offline disks, cluster shared disks or disks explicitly excluded by user will not be used by Windows. Ensure that disks are online and no disks are excluded by mistake." },
    { 0x80042415u, 0, "VSS_E_ASRERROR_CRITICAL_DISK_CANNOT_BE_EXCLUDED", "Restore failed because a disk which was critical at backup is excluded. To continue you need to either remove the disk from exclusion list or detach it from machine or clean it using DiskPart utility, and then retry restore. If you cannot clean or detach it then change the disk signature using DiskPart command UNIQUEID DISK ID." },
    { 0x80042416u, 0, "VSS_E_ASRERROR_SYSTEM_PARTITION_HIDDEN", "System partition (partition marked \"active\") is hidden or contains an unrecognized file system. Backup does not support this configuration." },
    { 0x80042417u, 0, "VSS_E_FSS_TIMEOUT", "A timeout occurred while preparing a file share shadowcopy for backup." },
};

const TableSpan g_HResultShadowCopyTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"vsserror.h"
};
