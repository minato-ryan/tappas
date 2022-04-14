import hashlib
import logging
import os
import subprocess
import tarfile
from abc import ABC, abstractmethod
from pathlib import Path

from config import config
from models import FolderRequirements


class Downloader(ABC):
    COMMON_SUFFIX = ['.hef', '.mp4']
    COMMON_RESOURCES_PATH = Path(__file__).parent.parent.resolve() / Path("apps/gstreamer/resources")
    TAPPAS_BUCKET = 'tappas'
    MODEL_ZOO_BUCKET = 'model_zoo'

    def __init__(self, root_path=None):
        self._md5_cache_dict = dict()
        self._logger = logging.getLogger(__file__)
        self._root_path = root_path or config.ROOT_PATH

    @abstractmethod
    def _download(self, requirement, destination, remote_md5):
        pass

    @abstractmethod
    def _get_md5(self, path):
        pass

    @staticmethod
    def get_folders_requirements():
        requirements = []

        for requirements_file in config.REQUIREMENTS_FILES:
            requirements_file_content = (config.REQUIREMENTS_PATH / requirements_file).read_text()
            requirements.append(FolderRequirements.parse_raw(requirements_file_content))

        return requirements

    def _requirements_manipulation(self, requirements):
        pass

    def run(self, init_common_dir=True):
        requirements = self.get_folders_requirements()
        self._requirements_manipulation(requirements)

        if init_common_dir:
            self._download_common_resources(requirements)

        for requirement in requirements:
            self._download_folder_requirements(requirement)

    def _extract_tar(self, tar_path, extract_to):
        with tarfile.open(tar_path, "r:gz") as tar_file:
            tar_file.extractall(path=extract_to)

    def _download_folder_requirements(self, folder_requirements: FolderRequirements):
        project_dir = self._root_path / folder_requirements.path

        for requirement in folder_requirements.requirements:
            destination_path = project_dir / requirement.destination
            destination_path.parent.mkdir(parents=True, exist_ok=True)

            self._download_file(destination_path, requirement)

    def _calculate_md5(self, file_path):
        md5 = hashlib.md5(file_path.read_bytes())
        md5 = md5.hexdigest()

        return md5

    def _update_md5_cache(self, file_path):
        md5 = self._calculate_md5(file_path)
        self._md5_cache_dict[md5] = file_path

    def _download_file(self, destination_path, requirement):
        if destination_path.is_file():
            self._logger.info(
                f'{destination_path.name} already exists inside {destination_path.parent}. Skipping download')
            self._update_md5_cache(destination_path)
        else:
            md5 = self._get_md5(requirement)

            if md5 in self._md5_cache_dict:
                self._logger.info(f'Creating softlink {self._md5_cache_dict[md5]} to {destination_path}')
                create_symlink_command = f"ln -s {self._md5_cache_dict[md5]} {destination_path}"
                subprocess.run(create_symlink_command.split(), check=True)
            else:
                self._logger.info(f'Downloading {requirement.source} into {destination_path}')
                self._download(requirement=requirement, destination=destination_path, remote_md5=md5)
                self._update_md5_cache(destination_path)

                if requirement.should_extract:
                    self._logger.info(f'Extracting {destination_path} to folder')
                    self._extract_tar(destination_path, destination_path.parent)
                    os.remove(destination_path)

    def _adjust_model_zoo_requirements(self, folders_requirements):
        """
        Until the ADK will upload their HEF's to the model-zoo, we have to copy them manually.
        This function adjust the hef paths
        """
        for folder_requirement in folders_requirements:
            model_zoo_requirements = filter(lambda req: req.bucket == self.MODEL_ZOO_BUCKET,
                                            folder_requirement.requirements)

            for requirement in model_zoo_requirements:
                network_name = Path(requirement.source).stem
                requirement.source = f"{network_name}/{network_name}.hef"

    def _get_requirements_by_suffix(self, folder_requirements):
        requirements_by_suffix_dict = dict()

        for folder_requirement in folder_requirements:
            for requirement in folder_requirement.requirements:
                requirement_source = Path(requirement.source)
                requirement_suffix = requirement_source.suffix

                if requirement_suffix not in self.COMMON_SUFFIX:
                    continue

                if requirement_suffix not in requirements_by_suffix_dict:
                    requirements_by_suffix_dict[requirement_suffix] = set()

                requirements_by_suffix_dict[requirement_suffix].add(requirement)

        return requirements_by_suffix_dict

    def _download_common_resources(self, requirements):
        requirements_by_suffix_dict = self._get_requirements_by_suffix(requirements)

        for suffix, requirements in requirements_by_suffix_dict.items():
            # The first char stores a dot, for example ".hef" --> "hef"
            current_suffix_resources_dir = self.COMMON_RESOURCES_PATH / suffix[1:]
            current_suffix_resources_dir.mkdir(parents=True, exist_ok=True)

            for requirement in requirements:
                destination_path = current_suffix_resources_dir / Path(requirement.destination).name
                self._download_file(destination_path=destination_path, requirement=requirement)
