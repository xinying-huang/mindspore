/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/graph_kernel/converter/akg/akg_build.h"

#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include "common/graph_kernel/core/graph_kernel_utils.h"
#include "kernel/akg/akg_kernel_json_generator.h"
#include "ir/anf.h"
#include "ir/func_graph.h"
#include "utils/anf_utils.h"
#include "utils/file_utils.h"
#include "utils/log_adapter.h"

namespace mindspore::graphkernel {
bool CompileSingleJson(const std::string &json_name) {
  std::string attrs = "None";
  std::ostringstream py_cmd;
  py_cmd << "from mindspore._extends.parallel_compile.akg_compiler.get_file_path import get_akg_path\n";
  py_cmd << "import sys\n";
  py_cmd << "sys.path.insert(0, get_akg_path())\n";
  py_cmd << "from akg.ms import compilewithjsonname\n";
  py_cmd << "if not compilewithjsonname(\'" << json_name << "\', " << attrs << "):\n";
  py_cmd << "    raise RuntimeError(\'Compile fail for json: " << json_name << "\')";
  std::string cmd = "unset LD_LIBRARY_PATH;python -c \"" + py_cmd.str() + "\"";
  auto ret = system(cmd.c_str());
  if (!WIFEXITED(ret)) {
    MS_LOG(ERROR) << "Python process start fail! process content is as follows:\n" << cmd;
    return false;
  }
  if (WEXITSTATUS(ret) != 0) {
    MS_LOG(ERROR) << "Failed to compile json: " << json_name;
    return false;
  }
  return true;
}

bool RetStatus(const int status) {
  if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) == 0) {
      MS_LOG(INFO) << "compile all pass for subprocess!";
      return true;
    } else {
      MS_LOG(ERROR) << "Some jsons compile fail, please check log!";
    }
  } else if (WIFSIGNALED(status)) {
    MS_LOG(ERROR) << "compile stopped by signal, maybe cost too long time!";
  } else if (WSTOPSIG(status)) {
    MS_LOG(ERROR) << "compile process is stopped by others!";
  } else {
    MS_LOG(ERROR) << "unknown error in compiling!";
  }
  return false;
}

bool CompileJsonsInList(const std::string &dir_path, const std::vector<std::string> &json_list) {
  size_t i;
  pid_t pid;
  std::vector<pid_t> child_process;
  for (i = 0; i < PROCESS_LIMIT; ++i) {
    pid = fork();
    if (pid < 0) {
      MS_LOG(ERROR) << "fork error";
      return false;
    } else if (pid == 0) {
      break;
    } else {
      child_process.emplace_back(pid);
    }
  }
  if (pid == 0) {
    setpgrp();
    (void)alarm(TIME_OUT);
    bool all_pass{true};
    for (size_t j = i; j < json_list.size(); j += PROCESS_LIMIT) {
      auto res = CompileSingleJson(dir_path + "/" + json_list[j] + ".info");
      if (!res) {
        all_pass = false;
      }
    }
    if (all_pass) {
      exit(0);
    } else {
      exit(1);
    }
  } else {
    bool all_process_pass{true};
    for (size_t j = 0; j < PROCESS_LIMIT; ++j) {
      int status = 0;
      waitpid(child_process[j], &status, 0);
      // kill child process of child process if overtime
      kill(-child_process[j], SIGTERM);
      all_process_pass = RetStatus(status) && all_process_pass;
    }
    if (all_process_pass) {
      return true;
    }
  }
  return false;
}

bool SaveJsonInfo(const std::string &json_name, const std::string &info) {
  std::string path = json_name + ".info";
  std::ofstream filewrite(path);
  if (!filewrite.is_open()) {
    MS_LOG(ERROR) << "Open file '" << path << "' failed!";
    return false;
  }
  filewrite << info << std::endl;
  filewrite.close();
  return true;
}

void CheckObjFiles(const std::string &dir_path, const std::vector<std::string> &json_list) {
  constexpr size_t try_times = 10;
  constexpr size_t wait_us = 100000;
  for (auto const json_name : json_list) {
    auto file_name = dir_path + "/" + json_name + ".json";
    bool exist = false;
    for (size_t i = 0; i < try_times; ++i) {
      std::ifstream f(file_name.c_str());
      if (f.good()) {
        exist = true;
        break;
      }
      usleep(wait_us);
    }
    if (!exist) {
      MS_LOG(EXCEPTION) << "akg file " << json_name << ".json not exist!";
    }
  }
}

std::string SaveNodesInfo(const AnfNodePtrList &nodes, const std::string &dir,
                          std::map<AnfNodePtr, std::string> *node_kernel, std::vector<std::string> *json_list) {
  auto dir_path = FileUtils::CreateNotExistDirs(dir);
  if (!dir_path.has_value()) {
    MS_LOG(ERROR) << "Failed to CreateNotExistDirs: " << dir;
    return "";
  }
  std::vector<std::string> kernel_names;
  for (const auto &node : nodes) {
    graphkernel::DumpOption option;
    option.get_target_info = true;
    graphkernel::AkgKernelJsonGenerator akg_kernel_json_generator(option);
    auto fg = GetCNodeFuncGraph(node);
    MS_EXCEPTION_IF_NULL(fg);
    auto mng = fg->manager();
    if (mng == nullptr) {
      mng = Manage(fg, true);
      fg->set_manager(mng);
    }
    std::vector<AnfNodePtr> node_list, input_list, output_list;
    GkUtils::GetValidKernelNodes(fg, &node_list, &input_list, &output_list);
    akg_kernel_json_generator.CollectFusedJson(node_list, input_list, output_list);
    auto json_kernel_name = akg_kernel_json_generator.kernel_name();
    if (node_kernel != nullptr) {
      (*node_kernel)[node] = json_kernel_name;
    }
    if (find(kernel_names.cbegin(), kernel_names.cend(), json_kernel_name) != kernel_names.cend()) {
      continue;
    }
    kernel_names.push_back(json_kernel_name);
    if (!SaveJsonInfo(dir_path.value() + "/" + json_kernel_name, akg_kernel_json_generator.kernel_json_str())) {
      return "";
    }
  }
  if (json_list != nullptr) {
    *json_list = std::move(kernel_names);
  }
  return dir_path.value();
}

bool AkgKernelBuilder::CompileJsonsInAnfnodes(const AnfNodePtrList &node_list) {
  std::map<AnfNodePtr, std::string> node_name;
  std::vector<std::string> json_list;
  auto dir_path = SaveNodesInfo(node_list, "./kernel_meta", &node_name, &json_list);
  if (dir_path.empty()) {
    return false;
  }
  auto res = CompileJsonsInList(dir_path, json_list);
  if (res) {
    for (const auto &iter : node_name) {
      AnfUtils::SetNodeAttr("kernel_name", MakeValue(iter.second + "_kernel"), iter.first);
    }
    std::ostringstream kernels_name;
    for (const auto &json_kernel_name : json_list) {
      kernels_name << dir_path << "/" << json_kernel_name << ".o ";
    }
    CheckObjFiles(dir_path, json_list);
    auto cmd = "g++ -fPIC -shared -o akgkernels.so " + kernels_name.str();
    if (system(cmd.c_str()) == 0) {
      return true;
    }
  }
  return false;
}
}  // namespace mindspore::graphkernel
