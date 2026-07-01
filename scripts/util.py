#!/usr/bin python3

from collections import defaultdict
from pathlib import Path


def get_directories(path: str) -> list[Path]:
    directory = Path(path)
    return [p for p in directory.iterdir()]


def all_treatments_have_same_subjects(
    treatments_to_subject_dirs: dict[str, list[Path]],
) -> bool:
    subject_program_names = []
    for _, subject_dirs in treatments_to_subject_dirs.items():
        subject_program_names.append({subject_dir.name for subject_dir in subject_dirs})
    subject_programs_for_experiments = subject_program_names[0]
    return all(v == subject_programs_for_experiments for v in subject_program_names)


def file_to_treatment_verification_results(
    treatment_to_file_verification_results: dict[str, list],
) -> dict[str, dict]:
    file_to_results = defaultdict(dict)
    for subject_program_results in treatment_to_file_verification_results.values():
        for file_verification_results in subject_program_results:
            for file_verification_result in file_verification_results:
                file_to_results[file_verification_result.file_name][
                    file_verification_result.treatment
                ] = file_verification_result
    return dict(file_to_results)
