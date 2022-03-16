//
// Created by Unravel on 2022/3/14.
//


#include "FuncInfo.h"
#include "GlobalInstance.h"
#include "FuncInfoGen.h"
#include <llvm/ADT/StringRef.h>
#include <string>

namespace wfg {

    vector<pair<unsigned, unsigned >>
    FuncInfoGenConsumer::findSensitiveLines(const SourceLocation &beginLoc, const SourceLocation &endLoc) const {
        FileID fileId = _manager.getMainFileID();
        StringRef funcContent{_manager.getCharacterData(beginLoc),
                           _manager.getCharacterData(endLoc) - _manager.getCharacterData(beginLoc) + 1};
        unsigned fileOffset = _manager.getFileOffset(beginLoc);
        vector<pair<unsigned,unsigned>> result;
        for (size_t i = 0; i < GlobalInstance::Config.keyWords.size(); ++i) {
            StringRef keyword{GlobalInstance::Config.keyWords[i]};
            size_t pos = 0;
            pos = funcContent.find(keyword, pos);
            while(pos != StringRef::npos) {
                unsigned line = _manager.getLineNumber(fileId, fileOffset + pos);
                result.emplace_back(line,i);
                pos += keyword.size();
                pos = funcContent.find(keyword, pos);
            }
        }
        return result;
    }

    void FuncInfoGenConsumer::travelCFGStmt(const Stmt *stmt, MiniCFG& miniCFG) {
        assert(stmt);
//        llvm::errs() << stmt->getStmtClassName() <<'\n';
        miniCFG.addASTStmtKind(stmt->getStmtClassName());
        for(auto it = stmt->child_begin(); it != stmt->child_end(); ++it) {
            travelCFGStmt(*it, miniCFG);
        }
    }

    void FuncInfoGenConsumer::catchSpecialStmt(const Stmt *stmt, MiniCFG &miniCfg) {
        if(stmt){
//            llvm::errs() <<"**"<< stmt->getStmtClassName() <<'\n';
            miniCfg.addASTStmtKind(stmt->getStmtClassName());
        }
    }

    MiniCFG FuncInfoGenConsumer::buildMiniCFG(FunctionDecl *funcDecl) const {
        Stmt* funcBody = funcDecl->getBody();
        unique_ptr<CFG> wholeCFG = CFG::buildCFG(funcDecl, funcBody, &_context, CFG::BuildOptions());
        MiniCFG miniCFG(funcDecl->getQualifiedNameAsString(), wholeCFG->size(),
                        GlobalInstance::Config.ASTStmtKindMap);

        for(auto& block: *wholeCFG) {
            unsigned cur = block->getBlockID();

            for(auto it = block->succ_begin();it != block->succ_end(); ++it) {
                CFGBlock *b = *it;
                if (!b) {
                    b = it->getPossiblyUnreachableBlock();
                }
                miniCFG.addSuccEdge(cur, b->getBlockID());
            }
            miniCFG.finishSuccEdges();

            for(auto it = block->pred_begin(); it!=block->pred_end(); ++it) {
                CFGBlock*b =*it;
                if(!b){
                    b = it->getPossiblyUnreachableBlock();
                }
                miniCFG.addPredEdge(cur, b->getBlockID());
            }
            miniCFG.finishPredEdges();

            for(const CFGElement& element: *block) {
                if(Optional<CFGStmt> cfgStmt = element.getAs<CFGStmt>()) {
                    const Stmt *stmt = cfgStmt->getStmt();
                    assert(stmt!= nullptr);
                    travelCFGStmt(stmt, miniCFG);
                }
            }
            catchSpecialStmt(block->getTerminatorStmt(), miniCFG);
            catchSpecialStmt(block->getLoopTarget(), miniCFG);
            catchSpecialStmt(block->getLabel(), miniCFG);

        }
        return miniCFG;
    }

    bool FuncInfoGenConsumer::VisitFunctionDecl(FunctionDecl *funcDecl) {
        if (funcDecl->doesThisDeclarationHaveABody() &&
            _manager.getFileID(funcDecl->getSourceRange().getBegin()) == _manager.getMainFileID()) {
            const string funcName = funcDecl->getQualifiedNameAsString();
            if(GlobalInstance::Config.matchFuncPrefix(funcName)) {
                FullSourceLoc beginLoc = _context.getFullLoc(funcDecl->getSourceRange().getBegin());
                FullSourceLoc endLoc = _context.getFullLoc(funcDecl->getSourceRange().getEnd());

                FuncInfo funcInfo(funcDecl->getQualifiedNameAsString(), beginLoc.getSpellingLineNumber(),
                                  endLoc.getSpellingLineNumber(), move(buildMiniCFG(funcDecl)));
                funcInfo.setSensitiveLines(std::move(findSensitiveLines(beginLoc,endLoc)));
                GlobalInstance::FuncInfoList.push_back(funcInfo);
            }
        }
        return true;
    }
}
