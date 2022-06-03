Value * ErrorV(const char *str) { printf("Error:\n%s\n",str); return 0; }
Value * ErrorV(string str) { cout << "Error:\n" << str << "\n"; return 0; }

static Module * module;
static map<string, Value *> NamedValues;
static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
Constant *CalleeF;

// constructor
CodeGen::CodeGen()
{
    module = new Module("flatB", TheContext);
    CalleeF = dyn_cast<Constant>(module->getOrInsertFunction("printf",FunctionType::get(IntegerType::getInt32Ty(TheContext), PointerType::get(Type::getInt8Ty(TheContext), 0), true )).getCallee());
    load_variable = 0;
    is_condition = 0;
    is_expression = 0;
}

void CodeGen::dump()
{
    //printf("\n\n");
    //module->dump();
    module->print(llvm::errs(), nullptr);
}

// helper to get expression
Value * CodeGen::get_expression()
{
    Value * v = ret;
    if(load_variable)
    {
        v = Builder.CreateLoad(v);
        load_variable = 0;
    }
    if(is_condition)
        v = Builder.CreateIntCast(v, Type::getInt32Ty(TheContext), true);

    is_condition = 0;
    is_expression = 1;
    return v;
}

// helper to get expression
Value * CodeGen::get_condition()
{
    Value * v = ret;
    if(load_variable)
    {
        v = Builder.CreateLoad(v);
        load_variable = 0;
    }
    if(is_expression)
        v = Builder.CreateICmpNE(v, ConstantInt::get(Type::getInt32Ty(TheContext), 0, true));

    is_condition = 1;
    is_expression = 0;
    return v;
}

// program
int CodeGen::visit(AST_program * program)
{
    FunctionType *ftype = FunctionType::get(Type::getVoidTy(TheContext), false);
    main_function = Function::Create(ftype, GlobalValue::ExternalLinkage, "main", module);
    program->decl_block->accept(*this);

    BasicBlock * BB = BasicBlock::Create(TheContext, "entry", main_function);
    Builder.SetInsertPoint(BB);
    program->code_block->accept(*this);

    Builder.CreateRetVoid();
    return 0;
}

// decl_block
int CodeGen::visit(AST_decl_block * decl_block)
{
    Type * ty = Type::getInt32Ty(TheContext);

    for(int i = 0; i < (int)decl_block->single_ints.size(); i++)
    {
        /*
        PointerType * ptr_typ = PointerType::get(ty, 0);
        GlobalVariable * gv = new GlobalVariable(*module, ptr_typ, false, GlobalValue::ExternalLinkage, 0, decl_block->single_ints[i]);
        gv->setInitializer(ConstantInt::get(module->getContext(), APInt(32, 0)));
        */
        module->getOrInsertGlobal(decl_block->single_ints[i],Builder.getInt32Ty());
        //PointerType* ptr_type = PointerType::get(ty,0);
        GlobalVariable* gv = module->getNamedGlobal(decl_block->single_ints[i]);
        gv->setLinkage(GlobalValue::CommonLinkage);
        gv->setInitializer(ConstantInt::get(TheContext, APInt(32,0)));
    }
    //cout << endl;

    //cout << "array ints : ";
    for(int i = 0; i < (int)decl_block->array_ints.size(); i++)
    {
        ArrayType * arr_type = ArrayType::get(ty, decl_block->array_ints[i].second);
        //PointerType * ptr_typ = PointerType::get(arr_type, 0);
        GlobalVariable * gv = new GlobalVariable(*module, arr_type, false, GlobalValue::ExternalLinkage, 0, decl_block->array_ints[i].first);
        gv->setInitializer(ConstantAggregateZero::get(arr_type));
    }
    return 0;
}

// code_block
int CodeGen::visit(AST_code_block * code_block)
{
    code_block->block_statement->accept(*this);
    load_variable = 0;
    return 0;
}

//
// all statements
//
int CodeGen::visit(AST_expression_statement * expression_statement)
{
    expression_statement->expression->accept(*this);
    get_expression();
    return 0;
}

int CodeGen::visit(AST_assignment_statement * assignment_statement)
{
    assignment_statement->expression->accept(*this);
    Value * expr_value = get_expression();

    assignment_statement->variable->accept(*this);
    Value * variable_value = ret;
    ret = Builder.CreateStore(expr_value, variable_value);

    load_variable = 0;
    return 0;
}

int CodeGen::visit(AST_block_statement * block_statement)
{
    for(int i = 0; i < (int)block_statement->statements.size(); i++)
        block_statement->statements[i]->accept(*this), load_variable = 0;
    return 0;
}

int CodeGen::visit(AST_if_statement * if_statement)
{
    Function * funct = Builder.GetInsertBlock()->getParent();
    BasicBlock * if_BB = BasicBlock::Create(TheContext, "if", funct);
    BasicBlock * next_BB = BasicBlock::Create(TheContext, "ifnext", funct);

    if_statement->condition->accept(*this);
    Value * condition_value = get_condition();
    //condition_value = Builder.CreateICmpEQ(condition_value, ConstantInt::get(Type::getInt1Ty(TheContext), true, true),"ifcond");
    Builder.CreateCondBr(condition_value, if_BB, next_BB);

    Builder.SetInsertPoint(if_BB);

    if_statement->if_block->accept(*this);
    //Value * if_value = ret;
    //if(load_variable){ if_value = Builder.CreateLoad(if_value); load_variable = 0;}
    Builder.CreateBr(next_BB);

    Builder.SetInsertPoint(next_BB);

    return 0;
}

int CodeGen::visit(AST_ifelse_statement * ifelse_statement)
{
    Function * funct = Builder.GetInsertBlock()->getParent();
    BasicBlock * if_BB = BasicBlock::Create(TheContext, "if", funct);
    BasicBlock * else_BB = BasicBlock::Create(TheContext, "else", funct);
    BasicBlock * next_BB = BasicBlock::Create(TheContext, "ifnext", funct);

    ifelse_statement->condition->accept(*this);
    Value * condition_value = get_condition();
    //if(load_variable){ condition_value = Builder.CreateLoad(condition_value); load_variable = 0;}
    //condition_value = Builder.CreateICmpEQ(condition_value, ConstantInt::get(Type::getInt1Ty(TheContext), true, true),"ifcond");

    Builder.CreateCondBr(condition_value, if_BB, else_BB);

    Builder.SetInsertPoint(if_BB);

    ifelse_statement->if_block->accept(*this);
    //Value * if_value = ret;
    //if(load_variable){ if_value = Builder.CreateLoad(if_value); load_variable = 0;}

    Builder.CreateBr(next_BB);
    //if_BB = Builder.GetInsertBlock();

    //funct->getBasicBlockList().push_back(else_BB);
    Builder.SetInsertPoint(else_BB);

    ifelse_statement->else_block->accept(*this);
    //Value * else_value = ret;
    //if(load_variable){ else_value = Builder.CreateLoad(else_value); load_variable = 0;}

    Builder.CreateBr(next_BB);
    //else_BB = Builder.GetInsertBlock();
    //funct->getBasicBlockList().push_back(next_BB);
    Builder.SetInsertPoint(next_BB);

    return 0;
}

int CodeGen::visit(AST_for_statement * for_statement)
{
    Function *funct = Builder.GetInsertBlock()->getParent();

    for_statement->from->accept(*this);
    Value * start = get_expression();

    for_statement->variable->accept(*this);
    Value * variable = ret;
    ret = Builder.CreateStore(start, variable);
    load_variable = 0;

    Value * cur_val = Builder.CreateLoad(variable);

    BasicBlock * for_condition_BB = BasicBlock::Create(TheContext, "for_condition", funct);
    BasicBlock * for_body_BB = BasicBlock::Create(TheContext, "for_body", funct);
    BasicBlock * for_after_BB = BasicBlock::Create(TheContext, "for_after", funct);
    BasicBlock * preheaderBB = Builder.GetInsertBlock();

    Builder.CreateBr(for_condition_BB);

    Builder.SetInsertPoint(for_condition_BB);

    PHINode * phi = Builder.CreatePHI(Type::getInt32Ty(TheContext), 2);
    phi->addIncoming(cur_val, preheaderBB);

    for_statement->to->accept(*this);
    Value * to_val = get_expression();

    //Value * cur_val = Builder.CreateLoad(variable);
    Value * cond = Builder.CreateICmpSLT(phi, to_val);
    Builder.CreateCondBr(cond, for_body_BB, for_after_BB);

    Builder.SetInsertPoint(for_body_BB);
    for_statement->for_block->accept(*this);
    //Value * body = ret;
    //if(load_variable){ body = Builder.CreateLoad(body); load_variable = 0;}

    for_statement->step->accept(*this);
    Value * step = get_expression();

    cur_val = Builder.CreateLoad(variable);
    cur_val = Builder.CreateAdd(cur_val, step);
    Builder.CreateStore(cur_val, variable);

    BasicBlock *loop_end_block = Builder.GetInsertBlock();

    Builder.CreateBr(for_condition_BB);

    Builder.SetInsertPoint(for_after_BB);

    phi->addIncoming(cur_val, loop_end_block);

    /*
    Function *funct = Builder.GetInsertBlock()->getParent();

    for_statement->from->accept(*this);
    Value * start = ret;
    if(load_variable){ start = Builder.CreateLoad(start); load_variable = 0;}

    for_statement->variable->accept(*this);
    Value * variable = ret;

    ret = Builder.CreateStore(start, variable);
    load_variable = 0;

    BasicBlock * for_condition_BB = BasicBlock::Create(TheContext, "for_condition", funct);
    BasicBlock * for_body_BB = BasicBlock::Create(TheContext, "for_body", funct);
    BasicBlock * for_after_BB = BasicBlock::Create(TheContext, "for_after", funct);

    Builder.CreateBr(for_condition_BB);

    Builder.SetInsertPoint(for_condition_BB);

    for_statement->to->accept(*this);
    Value * to_val = ret;
    if(load_variable){ to_val = Builder.CreateLoad(to_val); load_variable = 0;}

    Value * cur_val = Builder.CreateLoad(variable);
    Value * cond = Builder.CreateICmpSLT(cur_val, to_val);
    Builder.CreateCondBr(cond, for_body_BB, for_after_BB);

    Builder.SetInsertPoint(for_body_BB);
    for_statement->for_block->accept(*this);
    Value * body = ret;
    if(load_variable){ body = Builder.CreateLoad(body); load_variable = 0;}

    for_statement->step->accept(*this);
    Value * step = ret;
    if(load_variable){ step = Builder.CreateLoad(step); load_variable = 0;}

    cur_val = Builder.CreateLoad(variable);
    cur_val = Builder.CreateAdd(cur_val, step, "nextval");
    Builder.CreateStore(cur_val, variable);


    Builder.CreateBr(for_condition_BB);

    Builder.SetInsertPoint(for_after_BB);
    */
    return 0;
}

int CodeGen::visit(AST_while_statement * while_statement)
{
    Function *funct = Builder.GetInsertBlock()->getParent();

    Builder.GetInsertBlock();
    BasicBlock * cond_body = BasicBlock::Create(TheContext, "while_condition", funct);
    BasicBlock * loop_body = BasicBlock::Create(TheContext, "loop", funct);
    BasicBlock * afterBB = BasicBlock::Create(TheContext, "afterloop", funct);

    Builder.CreateBr(cond_body);
    Builder.SetInsertPoint(cond_body);
    while_statement->condition->accept(*this);
    Value * condition_value = get_condition();
    //condition_value = Builder.CreateICmpEQ(condition_value, ConstantInt::get(Type::getInt1Ty(TheContext), true, true),"ifcond");
    Builder.CreateCondBr(condition_value, loop_body, afterBB);

    Builder.SetInsertPoint(loop_body);
    while_statement->while_block->accept(*this);
    //Value * body = ret;
    //if(load_variable){ body = Builder.CreateLoad(body); load_variable = 0;}

    Builder.CreateBr(cond_body);
    Builder.GetInsertBlock();
    Builder.SetInsertPoint(afterBB);

    return 0;
}

int CodeGen::visit(AST_goto_statement * goto_statement)
{
    Value * cond;
    if(goto_statement->condition)
    {
        goto_statement->condition->accept(*this);
        cond = get_condition();
    }

    string & label = goto_statement->label;

    Function * funct = Builder.GetInsertBlock()->getParent();
    BasicBlock * label_BB;

    if(goto_labels.find(label) == goto_labels.end())
    {
        label_BB = BasicBlock::Create(TheContext, label, funct);
        goto_labels[label] = label_BB;
    }
    else
        label_BB = goto_labels[label];

    BasicBlock * next_BB = BasicBlock::Create(TheContext, "goto_next", funct);

    if(goto_statement->condition) Builder.CreateCondBr(cond, label_BB, next_BB);
    else Builder.CreateBr(label_BB);
    Builder.SetInsertPoint(next_BB);

    return 0;
}

int CodeGen::visit(AST_read_statement * read_statement)
{
    //cout << "read_statement" << endl;
    return 0;
}

int CodeGen::visit(AST_print_statement * print_statement)
{
    int sz = (int)print_statement->printables.size();
    for(int i = 0; i < sz; i++)
    {
        vector<Value *> args;

        AST_printable & p = print_statement->printables[i];
        if(p.expression)
        {
            p.expression->accept(*this);
            Value * v = get_expression();

            Value* val = Builder.CreateGlobalStringPtr("%d");
            args.push_back(val);
            args.push_back(v);
        }
        else
        {
            print_statement->printables[i].string_literal->accept(*this);
            Value* val = Builder.CreateGlobalStringPtr("%s");
            str_ = str_.substr(1, str_.length() - 2);
            Value* v = Builder.CreateGlobalStringPtr(str_);
            args.push_back(val);
            args.push_back(v);
        }
        ret =  Builder.CreateCall(CalleeF, args, "printfCall");

        args.clear();
        if(i != sz - 1)
            str_ = " ";
        else
            str_ = "\n";

        Value* val = Builder.CreateGlobalStringPtr("%s");
        Value* v = Builder.CreateGlobalStringPtr(str_);
        args.push_back(val);
        args.push_back(v);
        ret =  Builder.CreateCall(CalleeF, args, "printfCall");
    }
    return 0;
}

int CodeGen::visit(AST_label_statement * label_statement)
{
    string & label = label_statement->label;
    Function * funct = Builder.GetInsertBlock()->getParent();
    BasicBlock * label_BB;
    if(goto_labels.find(label) == goto_labels.end())
    {
        label_BB = BasicBlock::Create(TheContext, label, funct);
        goto_labels[label] = label_BB;
    }
    else
        label_BB = goto_labels[label];

    Builder.CreateBr(label_BB);
    Builder.SetInsertPoint(label_BB);
    return 0;
}


//
// all expressions
//
int CodeGen::visit(AST_binary_operator_expression * binary_operator_expression)
{
    int op = binary_operator_expression->op;

    binary_operator_expression->left->accept(*this);
    Value * L = get_expression();

    binary_operator_expression->right->accept(*this);
    Value * R = get_expression();

    is_condition = is_expression = 0;

    if(op == PLUS)  ret = Builder.CreateAdd(L, R, "addtmp"), is_expression = 1;
    else if(op == MINUS) ret = Builder.CreateSub(L, R, "subtmp"), is_expression = 1;
    else if(op == MULTIPLY) ret = Builder.CreateMul(L, R, "multmp"), is_expression = 1;
    else if(op == DIVIDE) ret = Builder.CreateSDiv(L, R, "divtmp"), is_expression = 1;
    else if(op == MODULO) ret = Builder.CreateSRem(L, R, "modtmp"), is_expression = 1;

    else if(op == LESS) ret = Builder.CreateICmpSLT(L, R, "lttmp"), is_condition = 1;
    else if(op == GREATER) ret = Builder.CreateICmpSGT(L, R, "gttmp"), is_condition = 1;
    else if(op == LESS_OR_EQUAL) ret = Builder.CreateICmpSLE(L, R, "letmp"), is_condition = 1;
    else if(op == GREATER_OR_EQUAL) ret = Builder.CreateICmpSGE(L, R, "getmp"), is_condition = 1;
    else if(op == EQUAL) ret = Builder.CreateICmpEQ(L, R, "eqtmp"), is_condition = 1;
    else if(op == NOT_EQUAL) ret = Builder.CreateICmpNE(L, R, "netmp"), is_condition = 1;
    else ret = ErrorV("Not a binary operator");

    load_variable = 0;
    return 0;
}

int CodeGen::visit(AST_unary_operator_expression * unary_operator_expression)
{
    int op = unary_operator_expression->op;

    unary_operator_expression->expression->accept(*this);
    Value * R = get_expression();

    if(op==UMINUS) ret = Builder.CreateNeg(R, "negtmp");
    else ret = ErrorV("Not a binary operator");

    is_expression = 1;
    is_condition = 0;
    load_variable = 0;
    return 0;
}


//
// variables and literals
//
int CodeGen::visit(AST_variable_single_int * variable_single_int)
{
    string & var_name = variable_single_int->variable_name;
    ret = module->getNamedGlobal(var_name);
    if(ret == NULL)
        ret = ErrorV("Unknown Variable name " + var_name);
    load_variable = 1;
    is_expression = 1;
    return 0;
}

int CodeGen::visit(AST_variable_array_int * variable_array_int)
{
    string & array_name = variable_array_int->array_name;

    variable_array_int->index->accept(*this);
    Value * index = get_expression();

    //index = ConstantInt::get(TheContext, APInt(32, 2));

    if(index == NULL)
        ret = ErrorV("Invalid Array Index in " + array_name);

    vector<Value*> array_index;
    array_index.push_back(Builder.getInt32(0));
    array_index.push_back(index);
    ret = Builder.CreateGEP(module->getNamedGlobal(array_name), array_index, array_name+"_IDX");
    load_variable = 1;
    is_expression = 1;
    //ret = Builder.CreateLoad(ret);
    return 0;
}

int CodeGen::visit(AST_int_literal * int_literal)
{
    is_expression = 1;
    ret = ConstantInt::get(TheContext, llvm::APInt(32, int_literal->int_literal));
    return 0;
}

int CodeGen::visit(AST_string_literal * string_literal)
{
    //ret = Builder.CreateGlobalStringPtr(string_literal->string_literal);
    str_ = string_literal->string_literal;
    return 0;
}
