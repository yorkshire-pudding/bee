<?php
/**
 * @file
 * Command(s) for downloading Backdrop projects.
 */

/**
 * Implements hook_bee_command().
 */
function download_bee_command() {
  return array(
    'download' => array(
      'description' => bt('Download Backdrop contrib projects.'),
      'callback' => 'download_bee_callback',
      'arguments' => array(
        'projects' => bt('One or more contrib projects to download.'),
      ),
      'multiple_argument' => 'projects',
      'options' => array(
        'hide-progress' => array(
          'description' => bt('Hide the download progress bar.'),
        ),
      ),
      'aliases' => array('dl', 'pm-download'),
      'examples' => array(
        'bee download webform' => bt('Download the Webform module.'),
        'bee download simplify thesis bamboo' => bt('Download the Simplify module, Thesis theme, and Bamboo layout.'),
        'bee download devel --hide-progress' => bt("Download the Devel module, and don't show the download progress bar."),
      ),
    ),
    'download-core' => array(
      'description' => bt('Download Backdrop core.'),
      'callback' => 'download_core_bee_callback',
      'arguments' => array(
        'directory' => bt('The directory to download and extract Backdrop into. Leave blank to use the current directory.'),
      ),
      'optional_arguments' => array('directory'),
      'options' => array(
        'hide-progress' => array(
          'description' => bt('Hide the download progress bar.'),
        ),
      ),
      'aliases' => array('dl-core'),
      'examples' => array(
        'bee download-core ../backdrop' => bt("Download Backdrop into a 'backdrop' directory in the parent folder."),
        'bee download-core --hide-progress' => bt("Download Backdrop into the current directory, and don't show the download progress bar."),
      ),
    ),
  );
}

/**
 * Command callback: Download Backdrop contrib projects.
 */
function download_bee_callback($arguments, $options) {
  global $_bee_backdrop_root, $_bee_backdrop_site;

  $progress = (!empty($options['hide-progress'])) ? FALSE : TRUE;

  // Get list of all projects so we don't try to download projects or
  // dependencies that already exist in core or contrib when we download a
  // named project.
  if (!empty($_bee_backdrop_root)) {
    // Get core and contrib by default.
    $locations = array('core', 'contrib');
    // Check whether we need to look in sites folders for contrib.
    (empty($_bee_backdrop_site)) ?: $locations[] = 'site_contrib';
    // Look in folders for core modules, themes and layouts.
    $project_types = array('module', 'theme', 'layout');
    // $project_types = array('theme');
    $existing_projects = array();
    foreach ($project_types as $project_type) {
      $projects = bee_find_all_projects($project_type, $locations);
      $existing_projects = array_merge($existing_projects, $projects);
      //print_r($projects);
      // foreach ($projects as &$existing_project) {
      //   $key = $existing_project['name'];
      //   $existing_projects[$key] = $existing_project;
      // }
    }
  }
  print_r($existing_projects);

  // Iterate through the list of projects manually so any dependencies can be
  // added to the list of projects to download.
  $project_count = 0;
  while ($project_count < count($arguments['projects'])) {
    $project = $arguments['projects'][$project_count];

    // Check if the project exists in the file system.
    if (array_key_exists($project, $existing_projects)) {
      $project_record = $existing_projects[$project];
      echo "Project Record: " . PHP_EOL;
      print_r($project_record);
      $skip_project = FALSE;
      switch ($project_record['location_category']) {
        case 'core':
          bee_message(bt("'!project' is a core !type and therefore already exists in '!location'.", array(
            '!project' => $project,
            '!location' => dirname($project_record['uri']),
            '!type' => $project_record['type'],
          )), 'status');
          $skip_project = TRUE;
          break;
        case 'contrib':
        case 'site_contrib':
          bee_message(bt("The '!project' !type already exists in '!location'.", array(
            '!project' => $project,
            '!location' => dirname($project_record['uri']),
            '!type' => $project_record['type'],
          )), 'status');
          $skip_project = TRUE;
          break;
      }
      if ($skip_project) {
        $project_count++;
        continue;
      }
    }

    // Check if the project exists by trying to get the repo homepage.
    $organisation = 'backdrop-contrib';
    $url = "https://github.com/$organisation/$project";
    $headers = get_headers($url);
    if (!$headers) {
      bee_message(bt("Unable to connect to !url.", array(
        '!url' => $url,
      )), 'error');
      continue;
    }
    $response = substr($headers[0], 9, 3);
    if ($response >= 400) {
      bee_message(bt("The '!project' project repository could not be found. Please check your spelling and try again.", array(
        '!project' => $project,
      )), 'error');
    }
    else {
      // Proceed if project is found.
      $info = (array) download_bee_git_info($project);

      // Get the list of dependencies and add to list of projects.
      if (!empty($info['dependencies'])) {
        $dependencies = $info['dependencies'];
        foreach ($dependencies as $dependency) {
          // Remove any minimum version requirements to get just the project
          // name.
          $dependency = explode(" ", $dependency, 2);
          $dependency = $dependency[0];
          // Check to see if the dependency already exists.
          if (in_array($dependency, $existing_projects)) {
            $project_record = $existing_projects[$dependency];
            switch ($project_record['location_category']) {
              case 'core':
                // Don't provide a message for core dependencies.
                continue 2;
              case 'contrib':
              case 'site_contrib':
                // Give a message to say it is a dependency but doesn't need to
                // be downloaded.
                bee_message(bt("'!dependency' is required by '!project' but already exists in '!location'.", array(
                  '!dependency' => $dependency,
                  '!project' => $project,
                  '!location' => $project_record['uri'],
                )), 'status');
                continue 2;
            }
          }
          else {
            // If it doesn't exist, add a message and add to the list of
            // projects.
            $arguments['projects'][] = $dependency;
            bee_message(bt("'!dependency' will also be downloaded, as it is required by '!project'.", array(
              '!dependency' => $dependency,
              '!project' => $project,
            )), 'status');
          }
        }
      }

      // Get the project type.
      if (empty($info['type'])) {
        bee_message(bt("The 'type' of project '!project' could not be determined.", array(
          '!project' => $project,
        )), 'error');
        return;
      }

      // Get the directory to download the project into.
      if (!empty($_bee_backdrop_site)) {
        $destination = "$_bee_backdrop_root/sites/$_bee_backdrop_site/"
          . $info['type']
          . 's';
      }
      elseif (!empty($_bee_backdrop_root)) {
        $destination = "$_bee_backdrop_root/" . $info['type'] . 's';
      }
      else {
        bee_message(bt("The download destination could not be determined. Re-run the command from within a Backdrop installation, or set the global '--root'/'--site' options."), 'error');
        return;
      }
      if (file_exists("$destination/contrib")) {
        $destination .= '/contrib';
      }
      $destination .= "/$project";
      if (!mkdir($destination, 0755, TRUE)) {
        bee_message(bt("The destination directory '!directory' doesn't exist and couldn't be created.", array(
          '!directory' => $destination,
        )), 'error');
        return;
      }

      // Download the project.
      if (download_bee_download_project($project, $info, $destination, $progress)) {
        bee_message(bt("'!project' was downloaded into '!directory'.", array(
          '!project' => $project,
          '!directory' => $destination,
        )), 'success');
        // Add the project and any submodules to the list of existing projects
        // in case they are dependencies for other projects in the download
        // list.
        $locations = array($destination);
        $downloaded_projects = bee_find_all_projects($info['type'], $locations);
        $existing_projects = array_merge($existing_projects, $downloaded_projects);
        // Once project is downloaded, check for any submodules.
        $submodule_path = $destination . '/modules';
        if (file_exists($submodule_path)) {
          // Get filename and project name for all submodules.
          $submodules = $downloaded_projects;
          unset($submodules['$project']);
          // Examine each submodule for dependencies.
          foreach ($submodules as $submodule) {
            $info_file = (string) file_get_contents($submodule['uri']);
            $info = (array) bee_parse_info_file($info_file);
            // Check to see if dependencies exist.
            if (!empty($info['dependencies'])) {
              foreach($info['dependencies'] as $dependency) {
                // Ignore if dependency is the parent project.
                if ($dependency == $project) {
                  continue;
                }
                if (in_array($dependency, $existing_projects)) {
                  $project_record = $existing_projects[$dependency];
                  switch ($project_record['location_category']) {
                    case 'core':
                      // Ignore if dependency is core module.
                      continue 2;
                    case 'contrib':
                    case 'site_contrib':
                      // Give a message to say it is a dependency but doesn't
                      // need to be downloaded.
                      bee_message(bt("'!dependency' is required by submodule '!submodule' of '!project' but already exists in '!location'.", array(
                        '!dependency' => $dependency,
                        '!submodule' => $submodule['name'],
                        '!project' => $project,
                        '!location' => $project_record['uri'],
                      )), 'status');
                      continue 2;
                  }
                }
                else {
                  // If it doesn't exist, add a message and add to the list of
                  // projects.
                  $arguments['projects'][] = $dependency;
                  bee_message(bt("'!dependency' will also be downloaded, as it is required by submodule '!submodule' of '!project'.", array(
                    '!dependency' => $dependency,
                    '!project' => $project,
                  )), 'status');
                }
              } // END foreach($info['dependencies'] as $dependency)
            }
          }
        }
      }
    }
    $project_count++;
  }
}

/**
 * Command callback: Download Backdrop core.
 */
function download_core_bee_callback($arguments, $options) {
  $info = download_bee_git_info('backdrop');
  $progress = (!empty($options['hide-progress'])) ? FALSE : TRUE;

  // Get or create the directory to download Backdrop into.
  $destination = !empty($arguments['directory']) ? $arguments['directory'] : getcwd();
  if (!realpath($destination)) {
    if (!mkdir($destination, 0755, TRUE)) {
      bee_message(bt("The destination directory '!directory' doesn't exist and couldn't be created.", array(
        '!directory' => $destination,
      )), 'error');
      return;
    }
  }
  $destination = realpath($destination);

  // Make sure the directory isn't an existing Backdrop installation.
  if (bee_find_root($destination)) {
    bee_message(bt("Backdrop cannot be downloaded into an existing installation. Re-run the command from outside the Backdrop installation, or set the download directory using the 'directory' argument."), 'error');
    return;
  }

  // Download Backdrop.
  if (download_bee_download_project('backdrop', $info, $destination, $progress)) {
    bee_message(bt("Backdrop was downloaded into '!directory'.", array(
      '!directory' => $destination,
    )), 'success');
  }
}

/**
 * Get information about a project from GitHub.
 *
 * @param string $project
 *   The name of the project.
 *
 * @return array
 *   An associative array of information about the project, possibly containing:
 *   - url: The URL to download the project.
 *   - branch: The default branch of the project.
 *   - type: The 'type' of project (e.g. module, theme or layout).
 */
function download_bee_git_info($project) {
  $info = array();
  $organisation = ($project == 'backdrop') ? 'backdrop' : 'backdrop-contrib';
  $api_url = 'https://api.github.com/repos';

  // Get the download URL of the latest release.
  $url = "https://github.com/$organisation/$project/releases/latest/download/$project.zip";
  $headers = get_headers($url);
  if (!$headers) {
    bee_message(bt("Unable to connect to !url.", array(
      '!url' => $url,
    )), 'error');
    return;
  }
  $response = substr($headers[0], 9, 3);
  if ($response < 400) {
    $info['url'] = $url;
  }
  else {
    // Offer to get the dev version instead.
    $answer = bee_confirm(bt("There is no release for '!organisation/!project'. Do you you want to download the dev version instead?", array(
      '!organisation' => $organisation,
      '!project' => $project,
    )));

    if ($answer) {
      // Get the download URL of the dev version.
      $repo_info = shell_exec("curl -sS $api_url/$organisation/$project");
      $repo_info = json_decode($repo_info, TRUE);
      $branch = $repo_info['default_branch'];
      $url = "https://github.com/$organisation/$project/archive/$branch.zip";
      $headers = get_headers($url);
      if (!$headers) {
        bee_message(bt("Unable to connect to !url.", array(
          '!url' => $url,
        )), 'error');
        return;
      }
      $response = substr($headers[0], 9, 3);
      if ($response < 400) {
        $info['url'] = $url;
        $info['branch'] = $branch;
      }
    }
  }

  // Get the 'name', 'type' and any 'dependencies' of project.
  if ($project != 'backdrop') {
    $files = shell_exec("curl -sS $api_url/$organisation/$project/contents");
    $files = json_decode($files, TRUE);
    foreach ($files as $file) {
      if ($file['type'] == 'file' && preg_match('/\.info$/i', $file['name'])) {
        $info_file = (string) shell_exec("curl -sSH 'Accept: application/vnd.github.v3.raw' $api_url/$organisation/$project/contents/" . $file['name']);
        $info = (array) bee_parse_info_file($info_file);
        break;
      }
    }
  }

  return $info;
}

/**
 * Download a project.
 *
 * @param string $project
 *   The name of the project to download.
 * @param array $info
 *   An associative array of information about the project from GitHub. It
 *   should contain:
 *   - url: The URL to download the project.
 *   - branch: The default branch of the project. Needed when downloading the
 *     dev version
 *   - type: The 'type' of project (e.g. module, theme or layout). Needed for
 *     contrib projects.
 * @param string $destination
 *   The path to the destination directory.
 * @param boolean $progress
 *   Whether or not the download progress bar should be displayed. Defaults to
 *   TRUE.
 *
 * @return boolean
 *   TRUE if the project was downloaded successfully, FALSE if not.
 */
function download_bee_download_project($project, array $info, $destination, $progress = TRUE) {
  // Get a temp directory.
  if (!$temp = bee_get_temp($project)) {
    bee_message(bt('Failed to get temp directory.'), 'error');
    return FALSE;
  }

  // Get the download URL.
  if (empty($url = $info['url'])) {
    bee_message(bt("The download URL for '!project' could not be found.", array(
      '!project' => $project,
    )), 'error');
    return FALSE;
  }

  // Download & extract the project.
  $file = "$temp/$project.zip";
  $directory = !empty($info['branch']) ? $project . '-' . $info['branch'] : $project;
  $wget = "wget";
  if ($progress) {
    // Check the wget version so the right "show progress" argument can be used.
    $wget_version_text = shell_exec("wget --version");
    $wget_version_number = substr((string) $wget_version_text, 9, 4);
    if (version_compare((string) $wget_version_number, "1.16", ">=")) {
      $wget_older = FALSE;
      $wget_progress_argument = " --show-progress -qO";
    }
    else {
      // Versions < 1.16 - different argument and no "quiet" argument is used.
      $wget_older = TRUE;
      $wget_progress_argument = " --progress=bar -O";
    }
    $wget .= $wget_progress_argument;
  }
  else {
    $wget .= " -qO";
  }
  $wget .= " $file $url";
  exec($wget);
  exec("unzip $file -d $temp");
  bee_copy("$temp/$directory", $destination, FALSE);
  bee_delete($temp);
  ($wget_older && $progress) ? bee_message(bt("The wget tool used by bee for downloading is version !version and does not support showing only the progress bar. Upgrade wget to version 1.16 or later; or use the '--hide-progress' argument if you wish to hide altogether.", array(
    '!version' => $wget_version_number,
  )), 'status') : '';
  return TRUE;
}

/**
 * List all projects and submodules in one or more locations scanning
 * recursively through folders.
 *
 * @param string $type
 *   The type of projects to find:
 *    - modules
 *    - themes
 *    - layouts
 *   These are the same regardless of core or contrib.
 *
 * @param array $locations
 *   The list of locations to check. One or more of:
 *    - core (./core/$type)
 *    - contrib (./$type)
 *    - site_contrib (./sites/$site_name/$type)
 *    - custom defined location (i.e. a file path)
 *   Multisite contrib search will need a search in both contrib and
 *   site_contrib locations; single site contrib will only need one location.
 *
 * @return array
 *   Returns an array of project names and their location together with the
 *   location type and project type.
 */
function bee_find_all_projects($type, array $locations) {
  global $_bee_backdrop_root, $_bee_backdrop_site;
  // Create an empty array for files.
  $files = array();
  // Set the pattern to look for .info files.
  $pattern = '#\.info$#';
  // Loop through each location.
  foreach ($locations as $location) {
    // Set the path depending on location.
    switch ($location) {
      case 'core':
        $path = "${_bee_backdrop_root}/core/${type}s";
        break;
      case 'contrib':
        $path = "${_bee_backdrop_root}/${type}s";
        break;
      case 'site_contrib':
        $path = "${_bee_backdrop_root}/sites/${_bee_backdrop_site}/${type}s";
        break;
      default:
        $path = $location;
        break;
    }
    // Scan the location recursively for projects.
    $options = array(
      'key' => 'name',
    );
    $files = bee_file_scan_directory($path, $pattern, $options);
    //print_r($files);
    // Modify each record to meet our needs.
    //$projects[] = array();
    foreach ($files as &$file) {
      // Add the revised project record to a common list of projects.
      $key = $file->name;
      //echo "$key ($file->uri)";
      $projects[$key] = array(
        'name' => $file->name,
        'uri' => $file->uri,
        'location_category' => $location,
        'type' => $type,
      );
    }
  }
  //print_r($projects);
  return $projects;
}

/**
 * Convert the contents of an info file into an array of the relevant parts.
 *
 * @param string $info_file
 *   The string contents of the project's .info file.
 *
 * @return array
 *   Returns an array of relevant .info fields.
 */
function bee_parse_info_file($info_file) {
  $info = array();
  // Split the text into lines.
  $lines = explode("\n", $info_file);
  // Declare dependencies as array.
  $info['dependencies'] = array();
  foreach ($lines as $line) {
    $values = explode('=', $line);
    // Get the name of the project.
    if (trim($values[0]) == 'name') {
      $info['name'] = trim($values[1]);
    }
    // Get the version of the project.
    if (trim($values[0]) == 'version') {
      $info['version'] = trim($values[1]);
    }
    // Get the type of project.
    if (trim($values[0]) == 'type') {
      $info['type'] = trim($values[1]);
    }
    // Get any dependencies of project.
    if (trim($values[0]) == 'dependencies[]') {
      $info['dependencies'][] = trim($values[1]);
    }
  }
  return $info;
}
