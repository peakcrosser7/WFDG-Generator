//
// Created by Unravel on 2022/4/12.
//

#ifndef WFG_GENERATOR_ABSTRACTDEPNHELPER_H
#define WFG_GENERATOR_ABSTRACTDEPNHELPER_H

#include "CustomCPG.h"
#include "DepnMapper.h"
#include "util.h"
#include <clang/AST/AST.h>
#include <clang/AST/ASTContext.h>
#include <unordered_map>
#include <utility>

using namespace clang;

namespace wfg {
    class AbstractDepnHelper {
    public:
        using VarIdType = DepnMapper::VarIdType;
        using VarIdPair = DepnMapper::VarIdPair;

    protected:
        CustomCPG &_customCPG;
        unsigned _nodeID;

        virtual void _doDepnOfReadRef(const DeclRefExpr *refExpr) = 0;

        virtual void _doDepnOfReadMember(const MemberExpr *memberExpr) = 0;

        virtual void _doDepnOfWrittenRef(const DeclRefExpr *refExpr) = 0;

        virtual void _doDepnOfWrittenMember(const MemberExpr *memberExpr) = 0;

        virtual void _doDepnOfRWVar(const Stmt *stmt) = 0;

        void _depnOfIncDecOp(const UnaryOperator *op);

        void _depnOfWrittenVar(const Stmt *writtenExpr, const Stmt *readExpr);

        void _buildDepn(const Stmt *stmt, bool canVisitCall = false);

        static VarIdPair _getRefVarIds(const DeclRefExpr *refExpr) {
            return make_pair(0, refExpr->getDecl()->getID());
        }

        static string _getStructIdsAndName(const MemberExpr *memberExpr, VarIdPair &ids) {
            const Stmt *childStmt = *(memberExpr->child_begin());
            while (!isa<DeclRefExpr>(childStmt) && !isa<MemberExpr>(childStmt)) {
                if (childStmt->child_begin() == childStmt->child_end()) {
                    return {};
                }
                childStmt = *(childStmt->child_begin());
            }
            string name{};
            if (isa<DeclRefExpr>(childStmt)) {
                const DeclRefExpr *refExpr = cast<DeclRefExpr>(childStmt);
                name.append(refExpr->getNameInfo().getAsString());
                ids = make_pair(refExpr->getDecl()->getID(), memberExpr->getMemberDecl()->getID());
            } else if (isa<MemberExpr>(childStmt)) {
                const MemberExpr *childMem = cast<MemberExpr>(childStmt);
                name.append(_getStructIdsAndName(childMem, ids));
            }
            name.append(memberExpr->isArrow() ? "->" : ".");
            name.append(memberExpr->getMemberDecl()->getNameAsString());
            return name;
        }

//        static bool _isStructDecl(const VarDecl *varDecl) {
//            const Type *type = varDecl->getType().getTypePtr();
//            while (type->isPointerType()) {
//                type = type->getPointeeOrArrayElementType();
//            }
//            return type->isStructureType();
//        }

    public:
        AbstractDepnHelper(CustomCPG &customCPG, unsigned nodeID)
                : _customCPG(customCPG), _nodeID(nodeID) {}

        void buildDepn(const Stmt *stmt) {
            _buildDepn(stmt, true);
        }

        virtual void depnOfDecl(const VarDecl *varDecl) = 0;

        void updateNodeID(unsigned nodeID) {
            _nodeID = nodeID;
        }
    };
}


#endif //WFG_GENERATOR_ABSTRACTDEPNHELPER_H
