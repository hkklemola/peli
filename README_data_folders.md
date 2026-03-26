# Data and Build Folder Conventions

## Current Folders
- **build-win/**: Windows build output (active)
- **build-lin/**: Linux build output (active)
- **backup_data/**: Stores backups of the data folder before cleaning or rebuilding

## Legacy Folders (can be deleted/ignored)
- **build/**: Old build output (legacy)
- **template_data/**: Old template data (legacy)

## Data Handling
- Always back up the data folder to backup_data/ before cleaning or rebuilding build-win/ or build-lin/.
- Only build-win/ and build-lin/ are used for current builds and runtime data.
- Do not use or update build/ or template_data/; these are for reference or safe deletion.
