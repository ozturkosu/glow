/**
 * Copyright (c) 2017-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GLOW_EXECUTIONENGINE_EXECUTIONENGINE_H
#define GLOW_EXECUTIONENGINE_EXECUTIONENGINE_H

#include "glow/Backends/Backend.h"
#include "glow/Backends/CompiledFunction.h"
#include "glow/Base/Train.h"
#include "glow/Base/Traits.h"
#include "glow/Graph/Context.h"
#include "glow/Graph/Graph.h"
#include "glow/Optimizer/Optimizer.h"

#include "llvm/ADT/ArrayRef.h"

#include <memory>
#include <unordered_map>

namespace glow {

/// This is the ExecutionEngine. It owns the Graph, the backend, and the
/// compiled function.  The Graph, etc in this class are defined as pointers, in
/// order to erase the type and prevent the internal types from leaking out to
/// the users of this class.
class ExecutionEngine final {
  /// The Module that represents the high-level program.
  Module M_;
  /// The network execution backend.
  Backend *backend_ = nullptr;
  /// Whether or not the ExecutionEngine owns the backend or is just using
  /// a backend provided from elsewhere. If ownsBackend is true,
  /// ~ExecutionEngine will delete the backend_.
  bool ownsBackend_ = false;
  /// Glow functions compiled for this ExecutionEngine's backend.
  llvm::StringMap<std::unique_ptr<CompiledFunction>> compiledFunctions_;

  /// Single execution of the given \compiledFunction with the given context
  /// \ctx.
  void runInternal(Context &ctx, CompiledFunction &compiledFunction);

public:
  ExecutionEngine(BackendKind backendKind = BackendKind::Interpreter);

  ~ExecutionEngine();

  /// Set the code generator kind to \p backendKind. New code will be generated
  /// using this backend.
  void setBackend(BackendKind backendKind);

  /// Set the code generator to a custom \p backend. If \p ownsBackend is false
  /// then ExecutionEngine will use the given backend without owning it which
  /// means that ~ExecutionEngine will not delete it.
  void setBackend(Backend *backend, bool ownsBackend = true);

  /// Get a pointer to the backend.
  const Backend *getBackend() const;

  /// \returns the internal graph.
  Module &getModule() { return M_; }

  /// \returns the compiled function. If more than one function
  /// has been compiled by this ExecutionEngine then a name must be supplied
  /// to specify which function to return.
  CompiledFunction &getCompiledFunction();

  /// \returns the compiled function with the given \p name.
  CompiledFunction &getCompiledFunction(llvm::StringRef name);

  /// \returns whether operation is supported by the underlying backend.
  bool isOpSupported(Kinded::Kind opKind, ElemKind elementTy) const {
    return backend_->isOpSupported(opKind, elementTy);
  }

  /// Optimize the Function \p f and pass it to the backend to compile it for a
  /// specific target. If \p clearOtherFunctions is false then the function will
  /// be added to the collection of previously compiled functions otherwise any
  /// previously compiled functions will be removed first. This method should be
  /// invoked before the run method.
  void compile(CompilationMode mode, Function *F,
               bool clearOtherFunctions = true);

  /// Save a bundle for a standalone execution. This method takes care of
  /// everything when preparing the bundle for saving. There is no need to
  /// invoke the compile method before it.
  /// Make \p networkName the function name for
  /// the entry point of the network and prepend all generated
  /// files with this name.
  void save(CompilationMode mode, Function *F, llvm::StringRef outputDir,
            llvm::StringRef networkName);

  /// Context aware single execution of a function. If more than one function
  /// has been compiled by this ExecutionEngine then a name must be supplied
  /// to specify which function to run.
  void run(Context &ctx);

  /// Context aware single execution of a function with the given \p name.
  void run(Context &ctx, llvm::StringRef name);
};

//===----------------------------------------------------------------------===//
//         Helper methods for running the execution engine.
//===----------------------------------------------------------------------===//

/// This method updates the placeholders in \p ph with the tensor content
/// values \p inputs, in \p ctx.
void updateInputPlaceholders(Context &ctx, llvm::ArrayRef<Placeholder *> ph,
                             llvm::ArrayRef<Tensor *> inputs);

/// This method updates the placeholders in the module. The placeholders are
/// found by name
///  in \p ph with the tensor content values \p inputs.
void updateInputPlaceholdersByName(Context &ctx, Module *mod,
                                   llvm::ArrayRef<llvm::StringRef> ph,
                                   llvm::ArrayRef<Tensor *> inputs);

/// Runs \p iterations iterations of the compiled function. The method updates a
/// global counter and future invocations of this method continue running
/// iterations of the batch at the next available slice.
///
/// The method updates the placeholder in \p ph with the tensors \p inputs. The
/// shape of the slice has to be identical to the shape of slices in the batch.
/// All dimensions, except for the first (batch) dimension must be identical.
///
/// The variable \p sampleCounter is consumed and updated by the function. This
/// variable records the number of samples that were consumed by the network in
/// previous iterations. The next input to be loaded is
/// (sampleCounter % batchsize).
void runBatch(ExecutionEngine &EE, Context &ctx, size_t iterations,
              size_t &sampleCounter, llvm::ArrayRef<Placeholder *> ph,
              llvm::ArrayRef<Tensor *> inputs);

} // namespace glow

#endif // GLOW_EXECUTIONENGINE_EXECUTIONENGINE_H
