#!/bin/bash

# Check if a filename is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

FILENAME=$1

# Confirm action with the user
read -p "Are you sure you want to remove all history of the file '$FILENAME' from the repository? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Operation cancelled."
    exit 1
fi

# Backup the repository
echo "Creating a backup of the repository..."
git clone --mirror . backup_repo.git
if [ $? -ne 0 ]; then
    echo "Failed to create a backup. Operation aborted."
    exit 1
fi

# Remove file history using git filter-branch
echo "Removing history of '$FILENAME'..."
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch $FILENAME" --prune-empty --tag-name-filter cat -- --all
if [ $? -ne 0 ]; then
    echo "Failed to remove file history. Operation aborted."
    exit 1
fi

# Clean up
echo "Cleaning up..."
rm -rf .git/refs/original/
git reflog expire --all --expire=now
git gc --prune=now --aggressive

# Force push changes to the remote repository
echo "Force pushing changes to the remote repository..."
git push origin --force --all
git push origin --force --tags
if [ $? -ne 0 ]; then
    echo "Failed to push changes to the remote repository. Operation aborted."
    exit 1
fi

echo "Successfully removed all history of '$FILENAME' and pushed changes to the remote repository."

# Clean up the backup repository
rm -rf backup_repo.git

exit 0
