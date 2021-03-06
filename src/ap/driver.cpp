/*
 * Automatic Parallelization using OpenMP
 *
 * Input: sequential C/C++ code
 * Output: parallelized C/C++ code using OpenMP
 *
 * Algorithm:
 *   Read in semantics specification (formerly array abstraction) files
 *   Collect all loops with canonical forms
 *     x. Conduct loop normalization
 *     x. Call dependence analysis from Qing's loop transformations
 *     x. Conduct liveness analysis and variable classification
 *     x. Judge if the loop is parallelizable
 *     x. Attach OmpAttribute if it is
 *     x. Insert OpenMP pragma accordingly
 *
 * By Chunhua Liao
 * Nov 3, 2008
 *
 * autoParallelization Project:
 * https://en.wikibooks.org/wiki/ROSE_Compiler_Framework/autoPar#Alternative
 *
 * autoPar.cc from rose/projects/autoParallelization
 * Originally by Chunhua Liao
 *
 *
 * -----------------------------------------------------------
 * Refactored and modified by Lichen Liu
 *
 */
#include "driver.h"
#include "ert_insertion.h"
#include "loop_analysis.h"
#include "config.hpp"
#include "utils.h"

#include <iostream>

namespace
{
    void findCandidateFunctionDefinitions(SgProject *project, std::vector<SgFunctionDefinition *> &candidateFuncDefs)
    {
        ROSE_ASSERT(project != nullptr);
        // For each source file in the project
        const std::vector<SgFile *> &ptr_list = project->get_fileList();
        for (SgFile *sageFile : ptr_list)
        {
            SgSourceFile *sfile = isSgSourceFile(sageFile);
            ROSE_ASSERT(sfile);

            if (AP::Config::get().enable_debug)
                std::cout << "Processing each function within the files " << sfile->get_file_info()->get_filename() << std::endl;

            std::vector<SgFunctionDefinition *> defList = SageInterface::querySubTree<SgFunctionDefinition>(sfile, V_SgFunctionDefinition);

            // For each function body in the scope
            for (SgFunctionDefinition *defn : defList)
            {
                SgFunctionDeclaration *func = defn->get_declaration();
                ROSE_ASSERT(func != nullptr);

                if (AP::Config::get().enable_debug)
                    std::cout << "\t considering function " << func->get_name() << " at " << func->get_file_info()->get_line() << std::endl;
                // ignore functions in system headers, Can keep them to test robustness
                if (defn->get_file_info()->get_filename() != sageFile->get_file_info()->get_filename())
                {
                    if (AP::Config::get().enable_debug)
                        std::cout << "\t Skipped since the function's associated file name does not match current file being considered. Mostly from a header. " << std::endl;
                    continue;
                }
                candidateFuncDefs.push_back(defn);
            } // end for def list
        }     // end for file list
    }

    // normalize all loops within candidate function definitions
    void normalizeLoops(std::vector<SgFunctionDefinition *> candidateFuncDefs)
    {
        for (SgFunctionDefinition *funcDef : candidateFuncDefs)
        {
            ROSE_ASSERT(funcDef);
            // This has to happen before analyses are called.
            // For each loop
            std::vector<SgForStatement *> loops = SageInterface::querySubTree<SgForStatement>(funcDef, V_SgForStatement);

            if (AP::Config::get().enable_debug)
                std::cout << "Normalize loops queried from memory pool ...." << std::endl;

            // normalize C99 style for (int i= x, ...) to C89 style: int i;  (i=x, ...)
            // Liao, 10/22/2009. Thank Jeff Keasler for spotting this bug
            for (SgForStatement *cur_loop : loops)
            {
                if (AP::Config::get().enable_debug)
                    std::cout << "\t loop at:" << cur_loop->get_file_info()->get_line() << std::endl;
                // skip for (;;) , SgForStatement::get_test_expr() has a buggy assertion.
                SgStatement *test_stmt = cur_loop->get_test();
                if (test_stmt != nullptr &&
                    isSgNullStatement(test_stmt))
                {
                    if (AP::Config::get().enable_debug)
                        std::cout << "\t skipped due to empty loop header like for (;;)" << std::endl;
                    continue;
                }

                // skip system header
                if (SageInterface::insideSystemHeader(cur_loop))
                {
                    if (AP::Config::get().enable_debug)
                        std::cout << "\t skipped since the loop is inside a system header " << std::endl;
                    continue;
                }
                SageInterface::forLoopNormalization(cur_loop);
            } // end for all loops
        }     // end for all function defs
    }
}

namespace AutoParallelization
{
    void auto_parallize(SgProject *project, int target_nthreads, AP::ERT_TYPE ert_type, bool enable_debug)
    {
        ROSE_ASSERT(project != nullptr);

        {
            AP::Config::get().enable_debug = enable_debug;
        }

        // create a block to avoid jump crosses initialization of candidateFuncDefs etc.
        {
            std::vector<SgFunctionDefinition *> candidateFuncDefs;
            findCandidateFunctionDefinitions(project, candidateFuncDefs);
            normalizeLoops(candidateFuncDefs);

            // Prepare liveness analysis etc.
            initialize_analysis(project, false);

            // This is a bit redundant with findCandidateFunctionDefinitions ()
            // But we do need the per file control to decide if omp.h is needed for each file
            //
            // For each source file in the project
            const std::vector<SgFile *> &ptr_list = project->get_fileList();
            for (SgFile *sageFile : ptr_list)
            {
                SgSourceFile *sfile = isSgSourceFile(sageFile);
                ROSE_ASSERT(sfile);

                std::vector<SgFunctionDefinition *> defList = SageInterface::querySubTree<SgFunctionDefinition>(sfile, V_SgFunctionDefinition);

                AP::SourceFileERTInserter sgfile_ert_inserter(sfile, ert_type);

                // For each function body in the scope
                for (SgFunctionDefinition *defn : defList)
                {
                    if (AP::Config::get().enable_debug)
                    {
                        std::cout << std::endl;
                        std::cout << std::endl;
                        std::cout << "===========================" << std::endl;
                        std::cout << "|| Function at line:" << defn->get_file_info()->get_line() << std::endl;
                        std::cout << "===========================" << std::endl;
                    }

                    SgFunctionDeclaration *func = defn->get_declaration();
                    ROSE_ASSERT(func != nullptr);

                    // ignore functions in system headers, Can keep them to test robustness
                    if (defn->get_file_info()->get_filename() != sageFile->get_file_info()->get_filename())
                    {
                        continue;
                    }

                    SgBasicBlock *body = defn->get_body();
                    // For each loop
                    std::vector<SgForStatement *> loops = SageInterface::querySubTree<SgForStatement>(defn, V_SgForStatement);
                    if (loops.size() == 0)
                    {
                        if (AP::Config::get().enable_debug)
                            std::cout << "\t skipped since no for loops are found in this function" << std::endl;
                        continue;
                    }

                    // X. Replace operators with their equivalent counterparts defined
                    // in "inline" annotations
                    AstInterfaceImpl faImpl_1(body);
                    CPPAstInterface fa_body(&faImpl_1);
                    OperatorInlineRewrite()(fa_body, AstNodePtrImpl(body));

                    // Pass annotations to arrayInterface and use them to collect
                    // alias info. function info etc.
                    ArrayAnnotation *annot = ArrayAnnotation::get_inst();
                    ArrayInterface array_interface(*annot);
                    // alias Collect
                    // value collect
                    array_interface.initialize(fa_body, AstNodePtrImpl(defn));
                    // valueCollect
                    array_interface.observe(fa_body);

                    // FR(06/07/2011): aliasinfo was not set which caused segfault
                    LoopTransformInterface::set_aliasInfo(&array_interface);

                    std::vector<SgForStatement *> parallelizable_loop_candidates;
                    for (SgForStatement *current_loop : loops)
                    {
                        if (AP::Config::get().enable_debug)
                        {
                            std::cout << std::endl;
                            std::cout << "\t\t ------------------------------" << std::endl;
                            std::cout << "\t\t | Considering loop at line:" << current_loop->get_file_info()->get_line() << std::endl;
                            std::cout << "\t\t ------------------------------" << std::endl;
                        }
                        // X. Parallelize loop one by one
                        //  getLoopInvariant() will actually check if the loop has canonical forms
                        //  which can be handled by dependence analysis

                        // skip loops with unsupported language features
                        VariantT blackConstruct;
                        if (useUnsupportedLanguageFeatures(current_loop, &blackConstruct))
                        {
                            if (AP::Config::get().enable_debug)
                                std::cout << "Skipping a loop at line:" << current_loop->get_file_info()->get_line() << " due to unsupported language construct " << blackConstruct << "..." << std::endl;
                            continue;
                        }

                        SgInitializedName *invarname = getLoopInvariant(current_loop);
                        if (invarname != nullptr)
                        {
                            bool ret = CanParallelizeOutermostLoop(current_loop, &array_interface, annot);
                            if (ret)
                            {
                                parallelizable_loop_candidates.emplace_back(current_loop);
                            }
                        }
                        else // cannot grab loop index from a non-conforming loop, skip parallelization
                        {
                            if (AP::Config::get().enable_debug)
                                std::cout << "Skipping a non-canonical loop at line:" << current_loop->get_file_info()->get_line() << "..." << std::endl;
                        }
                    } // end for loops

                    if (!parallelizable_loop_candidates.empty())
                    {
                        // Only parallelizable loops that are not nested inside any of other parallelizable loops are parallelized
                        std::vector<SgForStatement *> parallelizable_loop_final_candidates = AP::decideFinalLoopCandidates(parallelizable_loop_candidates);

                        // Parallelize loops
                        if (!parallelizable_loop_final_candidates.empty())
                        {
                            if (AP::Config::get().enable_debug)
                            {
                                std::cout << "-----------------------------------------------------" << std::endl;
                            }
                            sgfile_ert_inserter.insertERTIntoFunction(defn, target_nthreads);
                            for (SgForStatement *for_stmt : parallelizable_loop_final_candidates)
                            {
                                if (AP::Config::get().enable_debug)
                                {
                                    std::cout << "Automatically parallelized a loop at line:" << for_stmt->get_file_info()->get_line() << std::endl;
                                }
                                sgfile_ert_inserter.insertERTIntoForLoop(for_stmt);
                            }
                        }
                    }
                } // end for-loop for declarations

                if (sgfile_ert_inserter.is_ert_used())
                {
                    std::cout << std::endl;
                    std::cout << "=====================================================" << std::endl;
                    std::cout << "In source file: " << sfile->get_file_info()->get_filename() << std::endl;
                    std::cout << "Successfully found parallelizable loops and added Execution Runtime for parallelization!" << std::endl;
                }
            } // end for-loop of files

            // undo loop normalization
            for (auto [for_loop, _] : SageInterface::trans_records.forLoopInitNormalizationTable)
            {
                SageInterface::unnormalizeForLoopInitDeclaration(for_loop);
            }
            // Qing's loop normalization is not robust enough to pass all tests
            // AstTests::runAllTests(project);

            // clean up resources for analyses
            release_analysis();
        }
    }
}
