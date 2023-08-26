# Doxygen schema

| Key | Type | Description |
| --- | ---  |     ---     |
| `project_name` | string | Name of the project |
| `inputs`       | List of strings | List of directories or files to be scanned by Doxygen |
| `image_path`   | string | Relative location of path that contains images |

## Sample
```
doxygen:
  project_name: "My project"
  inputs:
    - inc
    - docs/doxygen_template.h
    - docs/doxygen_indexfile.md
  image_path: docs/images
  ```