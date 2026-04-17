# Data and Build Folder Conventions

## Current Folders
- **build-win/**: Windows build output and runtime data folder. The executable reads templates from `build-win/data/` when running from the build output.
- **build-lin/**: Linux build output and runtime data folder.
- **backup_data/**: Stores backups of the master_data folder before cleaning or rebuilding build-win/ or build-lin/.

## Master Template Folder
- **master_data/**: Master template source-of-truth. This folder contains the authoritative templates and is copied into build output folders.

## Legacy Folders (can be deleted/ignored)
- **build/**: Old build output (legacy)
- **template_data/**: Old template data (legacy)

## Data Handling
- Always back up the master_data folder to backup_data/ before cleaning or rebuilding build-win/ or build-lin/.
- Only build-win/ and build-lin/ are used for current builds and runtime data.
- Do not use or update build/ or template_data/; these are for reference or safe deletion.
