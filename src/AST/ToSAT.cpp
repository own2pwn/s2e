/********************************************************************
 * AUTHORS: Vijay Ganesh
 *
 * BEGIN DATE: November, 2005
 *
 * LICENSE: Please view LICENSE file in the home dir of this Program
 ********************************************************************/
// -*- c++ -*-
#include "AST.h"
#include "ASTUtil.h"
#include "../simplifier/bvsolver.h"
#include <math.h>

namespace BEEV
{
  /* FUNCTION: lookup or create a new MINISAT literal
   * lookup or create new MINISAT Vars from the global MAP
   * _ASTNode_to_SATVar.
   */
  const MINISAT::Var BeevMgr::LookupOrCreateSATVar(MINISAT::Solver& newS, const ASTNode& n)
  {
    ASTtoSATMap::iterator it;
    MINISAT::Var v;

    //look for the symbol in the global map from ASTNodes to ints. if
    //not found, create a S.newVar(), else use the existing one.
    if ((it = _ASTNode_to_SATVar.find(n)) == _ASTNode_to_SATVar.end())
      {
        v = newS.newVar();
        _ASTNode_to_SATVar[n] = v;

        //ASSUMPTION: I am assuming that the newS.newVar() call increments v
        //by 1 each time it is called, and the initial value of a
        //MINISAT::Var is 0.
        _SATVar_to_AST.push_back(n);
      }
    else
      v = it->second;
    return v;
  }

  /* FUNCTION: convert ASTClauses to MINISAT clauses and solve.
   * Accepts ASTClauses and converts them to MINISAT clauses. Then adds
   * the newly minted MINISAT clauses to the local SAT instance, and
   * calls solve(). If solve returns unsat, then stop and return
   * unsat. else continue.
   */
  // FIXME: Still need to deal with TRUE/FALSE in clauses!
  //bool BeevMgr::toSATandSolve(MINISAT::Solver& newS, BeevMgr::ClauseList& cll, ASTNodeToIntMap& heuristic)
  bool BeevMgr::toSATandSolve(MINISAT::Solver& newS, BeevMgr::ClauseList& cll)
  {
    CountersAndStats("SAT Solver");

    //iterate through the list (conjunction) of ASTclauses cll
    BeevMgr::ClauseList::const_iterator i = cll.begin(), iend = cll.end();

    if (i == iend)
      FatalError("toSATandSolve: Nothing to Solve", ASTUndefined);

    //turnOffSubsumption
    // MKK: My understanding is that the rougly equivalent effect is had
    // through setting the second argument of MINISAT::Solver::solve to
    // true
    //newS.turnOffSubsumption();

    // (*i) is an ASTVec-ptr which denotes an ASTclause
    //****************************************
    // *i = vector<const ASTNode*>*
    //****************************************
    for (; i != iend; i++)
      {
        //Clause for the SATSolver
        MINISAT::vec<MINISAT::Lit> satSolverClause;

        //now iterate through the internals of the ASTclause itself
        vector<const ASTNode*>::const_iterator j = (*i)->begin(), jend = (*i)->end();
        //j is a disjunct in the ASTclause (*i)
        for (; j != jend; j++)
          {
            ASTNode node = **j;

            bool negate = (NOT == node.GetKind()) ? true : false;
            ASTNode n = negate ? node[0] : node;

            //Lookup or create the MINISAT::Var corresponding to the Booelan
            //ASTNode Variable, and push into sat Solver clause
            MINISAT::Var v = LookupOrCreateSATVar(newS, n);
            MINISAT::Lit l(v, negate);
            satSolverClause.push(l);
          }
        newS.addClause(satSolverClause);
        // clause printing.
        // (printClause<MINISAT::vec<MINISAT::Lit> >)(satSolverClause);
        // cout << " 0 ";
        // cout << endl;

        if (newS.okay())
          {
            continue;
          }
        else
          {
            PrintStats(newS);
            return false;
          }

        if (!newS.simplify())
          {
            PrintStats(newS);
            return false;
          }
      }

    // if input is UNSAT return false, else return true
    if (!newS.simplify())
      {
        PrintStats(newS);
        return false;
      }

    //PrintActivityLevels_Of_SATVars("Before SAT:",newS);
    //ChangeActivityLevels_Of_SATVars(newS);
    //PrintActivityLevels_Of_SATVars("Before SAT and after initial bias:",newS);
    //newS.solve();
    newS.solve();
    //PrintActivityLevels_Of_SATVars("After SAT",newS);

    PrintStats(newS);
    if (newS.okay())
      return true;
    else
      return false;
  }

  // GLOBAL FUNCTION: Prints statistics from the MINISAT Solver
  void BeevMgr::PrintStats(MINISAT::Solver& s)
  {
    if (!stats_flag)
      return;
    double cpu_time = MINISAT::cpuTime();
    uint64_t mem_used = MINISAT::memUsed();
    reportf("restarts              : %llu\n",                      s.starts);
    reportf("conflicts             : %llu   (%.0f /sec)\n",        s.conflicts   , s.conflicts   /cpu_time);
    reportf("decisions             : %llu   (%.0f /sec)\n",        s.decisions   , s.decisions   /cpu_time);
    reportf("propagations          : %llu   (%.0f /sec)\n",        s.propagations, s.propagations/cpu_time);
    reportf("conflict literals     : %llu   (%4.2f %% deleted)\n", s.tot_literals,
            (s.max_literals - s.tot_literals)*100 / (double)s.max_literals);
    if (mem_used != 0)
      reportf("Memory used           : %.2f MB\n", mem_used / 1048576.0);
    reportf("CPU time              : %g s\n", cpu_time);
  }

  // Prints Satisfying assignment directly, for debugging.
  void BeevMgr::PrintSATModel(MINISAT::Solver& newS)
  {
    if (!newS.okay())
      FatalError("PrintSATModel: NO COUNTEREXAMPLE TO PRINT", ASTUndefined);
    // FIXME: Don't put tests like this in the print functions.  The print functions
    // should print unconditionally.  Put a conditional around the call if you don't
    // want them to print
    if (!(stats_flag && print_nodes_flag))
      return;

    int num_vars = newS.nVars();
    cout << "Satisfying assignment: " << endl;
    for (int i = 0; i < num_vars; i++)
      {
        if (newS.model[i] == MINISAT::l_True)
          {
            ASTNode s = _SATVar_to_AST[i];
            cout << s << endl;
          }
        else if (newS.model[i] == MINISAT::l_False)
          {
            ASTNode s = _SATVar_to_AST[i];
            cout << CreateNode(NOT, s) << endl;
          }
      }
  }

  // Looks up truth value of ASTNode SYMBOL in MINISAT satisfying assignment.
  // Returns ASTTrue if true, ASTFalse if false or undefined.
  ASTNode BeevMgr::SymbolTruthValue(MINISAT::Solver &newS, ASTNode form)
  {
    MINISAT::Var satvar = _ASTNode_to_SATVar[form];
    if (newS.model[satvar] == MINISAT::l_True)
      {
        return ASTTrue;
      }
    else
      {
        // False or undefined.
        return ASTFalse;
      }
  }

  // This function is for debugging problems with BitBlast and especially
  // ToCNF. It evaluates the bit-blasted formula in the satisfying
  // assignment.  While doing that, it checks that every subformula has
  // the same truth value as its representative literal, if it has one.
  // If this condition is violated, it halts immediately (on the leftmost
  // lowest term).
  // Use CreateSimpForm to evaluate, even though it's expensive, so that
  // we can use the partial truth assignment.
  ASTNode BeevMgr::CheckBBandCNF(MINISAT::Solver& newS, ASTNode form)
  {
    // Clear memo table (in case newS has changed).
    CheckBBandCNFMemo.clear();
    // Call recursive version that does the work.
    return CheckBBandCNF_int(newS, form);
  }

  // Recursive body CheckBBandCNF
  // FIXME:  Modify this to just check if result is true, and print mismatch
  // if not.   Might have a trace flag for the other stuff.
  ASTNode BeevMgr::CheckBBandCNF_int(MINISAT::Solver& newS, ASTNode form)
  {

    //    cout << "++++++++++++++++" << endl << "CheckBBandCNF_int form = " <<
    //      form << endl;

    ASTNodeMap::iterator memoit = CheckBBandCNFMemo.find(form);
    if (memoit != CheckBBandCNFMemo.end())
      {
        // found it.  Return memoized value.
        return memoit->second;
      }

    ASTNode result; // return value, to memoize.

    Kind k = form.GetKind();
    switch (k)
      {
      case TRUE:
      case FALSE:
        {
          return form;
          break;
        }
      case SYMBOL:
      case BVGETBIT:
        {
          // Look up the truth value
          // ASTNode -> Sat -> Truthvalue -> ASTTrue or ASTFalse;
          // FIXME: Could make up a fresh var in undefined case.

          result = SymbolTruthValue(newS, form);

          cout << "================" << endl << "Checking BB formula:" << form << endl;
          cout << "----------------" << endl << "Result:" << result << endl;

          break;
        }
      default:
        {
          // Evaluate the children recursively.
          ASTVec eval_children;
          ASTVec ch = form.GetChildren();
          ASTVec::iterator itend = ch.end();
          for (ASTVec::iterator it = ch.begin(); it < itend; it++)
            {
              eval_children.push_back(CheckBBandCNF_int(newS, *it));
            }
          result = CreateSimpForm(k, eval_children);

          cout << "================" << endl << "Checking BB formula:" << form << endl;
          cout << "----------------" << endl << "Result:" << result << endl;

          ASTNode replit_eval;
          // Compare with replit, if there is one.
          ASTNodeMap::iterator replit_it = RepLitMap.find(form);
          if (replit_it != RepLitMap.end())
            {
              ASTNode replit = RepLitMap[form];
              // Replit is symbol or not symbol.
              if (SYMBOL == replit.GetKind())
                {
                  replit_eval = SymbolTruthValue(newS, replit);
                }
              else
                {
                  // It's (NOT sym).  Get value of sym and complement.
                  replit_eval = CreateSimpNot(SymbolTruthValue(newS, replit[0]));
                }

              cout << "----------------" << endl << "Rep lit: " << replit << endl;
              cout << "----------------" << endl << "Rep lit value: " << replit_eval << endl;

              if (result != replit_eval)
                {
                  // Hit the panic button.
                  FatalError("Truth value of BitBlasted formula disagrees with representative literal in CNF.");
                }
            }
          else
            {
              cout << "----------------" << endl << "No rep lit" << endl;
            }

        }
      }

    return (CheckBBandCNFMemo[form] = result);
  }

  /*FUNCTION: constructs counterexample from MINISAT counterexample
   * step1 : iterate through MINISAT counterexample and assemble the
   * bits for each AST term. Store it in a map from ASTNode to vector
   * of bools (bits).
   *
   * step2: Iterate over the map from ASTNodes->Vector-of-Bools and
   * populate the CounterExampleMap data structure (ASTNode -> BVConst)
   */
  void BeevMgr::ConstructCounterExample(MINISAT::Solver& newS)
  {
    //iterate over MINISAT counterexample and construct a map from AST
    //terms to vector of bools. We need this iteration step because
    //MINISAT might return the various bits of a term out of
    //order. Therfore, we need to collect all the bits and assemble
    //them properly

    if (!newS.okay())
      return;
    if (!construct_counterexample_flag)
      return;

    CopySolverMap_To_CounterExample();
    for (int i = 0; i < newS.nVars(); i++)
      {
        //Make sure that the MINISAT::Var is defined
        if (newS.model[i] != MINISAT::l_Undef)
          {

            //mapping from MINISAT::Vars to ASTNodes. We do not need to
            //print MINISAT vars or CNF vars.
            ASTNode s = _SATVar_to_AST[i];

            //assemble the counterexample here
            if (s.GetKind() == BVGETBIT && s[0].GetKind() == SYMBOL)
              {
                ASTNode symbol = s[0];
                unsigned int symbolWidth = symbol.GetValueWidth();

                //'v' is the map from bit-index to bit-value
                hash_map<unsigned, bool> * v;
                if (_ASTNode_to_Bitvector.find(symbol) == _ASTNode_to_Bitvector.end())
                  _ASTNode_to_Bitvector[symbol] = new hash_map<unsigned, bool> (symbolWidth);

                //v holds the map from bit-index to bit-value
                v = _ASTNode_to_Bitvector[symbol];

                //kk is the index of BVGETBIT
                unsigned int kk = GetUnsignedConst(s[1]);

                //Collect the bits of 'symbol' and store in v. Store in reverse order.
                if (newS.model[i] == MINISAT::l_True)
                  (*v)[(symbolWidth - 1) - kk] = true;
                else
                  (*v)[(symbolWidth - 1) - kk] = false;
              }
            else
              {
                if (s.GetKind() == SYMBOL && s.GetType() == BOOLEAN_TYPE)
                  {
                    const char * zz = s.GetName();
                    //if the variables are not cnf variables then add them to the counterexample
                    if (0 != strncmp("cnf", zz, 3) && 0 != strcmp("*TrueDummy*", zz))
                      {
                        if (newS.model[i] == MINISAT::l_True)
                          CounterExampleMap[s] = ASTTrue;
                        else if (newS.model[i] == MINISAT::l_False)
                          CounterExampleMap[s] = ASTFalse;
                        else
                          {
                            int seed = 10000;
                            srand(seed);
                            CounterExampleMap[s] = (rand() > seed) ? ASTFalse : ASTTrue;
                          }
                      }
                  }
              }
          }
      }

    //iterate over the ASTNode_to_Bitvector data-struct and construct
    //the the aggregate value of the bitvector, and populate the
    //CounterExampleMap datastructure
    for (ASTtoBitvectorMap::iterator it = _ASTNode_to_Bitvector.begin(), itend = _ASTNode_to_Bitvector.end(); it != itend; it++)
      {
        ASTNode var = it->first;
        //debugging
        //cerr << var;
        if (SYMBOL != var.GetKind())
          FatalError("ConstructCounterExample: error while constructing counterexample: not a variable: ", var);

        //construct the bitvector value
        hash_map<unsigned, bool> * w = it->second;
        ASTNode value = BoolVectoBVConst(w, var.GetValueWidth());
        //debugging
        //cerr << value;

        //populate the counterexample datastructure. add only scalars
        //variables which were declared in the input and newly
        //introduced variables for array reads
        CounterExampleMap[var] = value;
      }

    //In this loop, we compute the value of each array read, the
    //corresponding ITE against the counterexample generated above.
    for (ASTNodeMap::iterator it = _arrayread_ite.begin(), itend = _arrayread_ite.end(); it != itend; it++)
      {
        //the array read
        ASTNode arrayread = it->first;
        ASTNode value_ite = _arrayread_ite[arrayread];

        //convert it to a constant array-read and store it in the
        //counter-example. First convert the index into a constant. then
        //construct the appropriate array-read and store it in the
        //counterexample
        ASTNode arrayread_index = TermToConstTermUsingModel(arrayread[1]);
        ASTNode key = CreateTerm(READ, arrayread.GetValueWidth(), arrayread[0], arrayread_index);

        //Get the ITE corresponding to the array-read and convert it
        //to a constant against the model
        ASTNode value = TermToConstTermUsingModel(value_ite);
        //save the result in the counter_example
        if (!CheckSubstitutionMap(key))
          CounterExampleMap[key] = value;
      }
  } //End of ConstructCounterExample


  // FUNCTION: accepts a non-constant term, and returns the
  // corresponding constant term with respect to a model.
  //
  // term READ(A,i) is treated as follows:
  //
  //1. If (the boolean variable 'ArrayReadFlag' is true && ArrayRead
  //1. has value in counterexample), then return the value of the
  //1. arrayread.
  //
  //2. If (the boolean variable 'ArrayReadFlag' is true && ArrayRead
  //2. doesn't have value in counterexample), then return the
  //2. arrayread itself (normalized such that arrayread has a constant
  //2. index)
  //
  //3. If (the boolean variable 'ArrayReadFlag' is false) && ArrayRead
  //3. has a value in the counterexample then return the value of the
  //3. arrayread.
  //
  //4. If (the boolean variable 'ArrayReadFlag' is false) && ArrayRead
  //4. doesn't have a value in the counterexample then return 0 as the
  //4. value of the arrayread.
  ASTNode BeevMgr::TermToConstTermUsingModel(const ASTNode& t, bool ArrayReadFlag)
  {
    Begin_RemoveWrites = false;
    SimplifyWrites_InPlace_Flag = false;
    //ASTNode term = SimplifyTerm(t);
    ASTNode term = t;
    Kind k = term.GetKind();

    //cerr << "Input to TermToConstTermUsingModel: " << term << endl;
    if (!is_Term_kind(k))
      {
        FatalError("TermToConstTermUsingModel: The input is not a term: ", term);
      }
    if (k == WRITE)
      {
        FatalError("TermToConstTermUsingModel: The input has wrong kind: WRITE : ", term);
      }
    if (k == SYMBOL && BOOLEAN_TYPE == term.GetType())
      {
        FatalError("TermToConstTermUsingModel: The input has wrong kind: Propositional variable : ", term);
      }

    ASTNodeMap::iterator it1;
    if ((it1 = CounterExampleMap.find(term)) != CounterExampleMap.end())
      {
        ASTNode val = it1->second;
        if (BVCONST != val.GetKind())
          {
            //CounterExampleMap has two maps rolled into
            //one. SubstitutionMap and SolverMap.
            //
            //recursion is fine here. There are two maps that are checked
            //here. One is the substitutionmap. We garuntee that the value
            //of a key in the substitutionmap is always a constant.
            //
            //in the SolverMap we garuntee that "term" does not occur in
            //the value part of the map
            if (term == val)
              {
                FatalError("TermToConstTermUsingModel: The input term is stored as-is "
                           "in the CounterExample: Not ok: ", term);
              }
            return TermToConstTermUsingModel(val, ArrayReadFlag);
          }
        else
          {
            return val;
          }
      }

    ASTNode output;
    switch (k)
      {
      case BVCONST:
        output = term;
        break;
      case SYMBOL:
        {
          if (term.GetType() == ARRAY_TYPE)
            {
              return term;
            }

          //when all else fails set symbol values to some constant by
          //default. if the variable is queried the second time then add 1
          //to and return the new value.
          ASTNode zero = CreateZeroConst(term.GetValueWidth());
          output = zero;
          break;
        }
      case READ:
        {
          ASTNode arrName = term[0];
          ASTNode index = term[1];
          if (0 == arrName.GetIndexWidth())
            {
              FatalError("TermToConstTermUsingModel: array has 0 index width: ", arrName);
            }


          if (WRITE == arrName.GetKind()) //READ over a WRITE
            {
              ASTNode wrtterm = Expand_ReadOverWrite_UsingModel(term, ArrayReadFlag);
              if (wrtterm == term)
                {
                  FatalError("TermToConstTermUsingModel: Read_Over_Write term must be expanded into an ITE", term);
                }
              ASTNode rtterm = TermToConstTermUsingModel(wrtterm, ArrayReadFlag);
              assert(ArrayReadFlag || (BVCONST == rtterm.GetKind()));
              return rtterm;
            }
          else if (ITE == arrName.GetKind()) //READ over an ITE
            {
              // The "then" and "else" branch are arrays.
              ASTNode indexVal = TermToConstTermUsingModel(index, ArrayReadFlag);

              ASTNode condcompute = ComputeFormulaUsingModel(arrName[0]); // Get the truth value.
              if (ASTTrue == condcompute)
                {
                  const ASTNode & result = TermToConstTermUsingModel(CreateTerm(READ, arrName.GetValueWidth(), arrName[1], indexVal), ArrayReadFlag);
                  assert(ArrayReadFlag || (BVCONST == result.GetKind()));
                  return result;
                }
              else if (ASTFalse == condcompute)
                {
                  const ASTNode & result =  TermToConstTermUsingModel(CreateTerm(READ, arrName.GetValueWidth(), arrName[2], indexVal), ArrayReadFlag);
                  assert(ArrayReadFlag || (BVCONST == result.GetKind()));
                  return result;
                }
              else
                {
                  cerr << "TermToConstTermUsingModel: termITE: value of conditional is wrong: " << condcompute << endl;
                  FatalError(" TermToConstTermUsingModel: termITE: cannot compute ITE conditional against model: ", term);
                }
              FatalError("bn23143 Never Here");
            }

          ASTNode modelentry;
          if (CounterExampleMap.find(index) != CounterExampleMap.end())
            {
              //index has a const value in the CounterExampleMap
              //ASTNode indexVal = CounterExampleMap[index];
              ASTNode indexVal = TermToConstTermUsingModel(CounterExampleMap[index], ArrayReadFlag);
              modelentry = CreateTerm(READ, arrName.GetValueWidth(), arrName, indexVal);
            }
          else
            {
              //index does not have a const value in the CounterExampleMap. compute it.
              ASTNode indexconstval = TermToConstTermUsingModel(index, ArrayReadFlag);
              //update model with value of the index
              //CounterExampleMap[index] = indexconstval;
              modelentry = CreateTerm(READ, arrName.GetValueWidth(), arrName, indexconstval);
            }
          //modelentry is now an arrayread over a constant index
          BVTypeCheck(modelentry);

          //if a value exists in the CounterExampleMap then return it
          if (CounterExampleMap.find(modelentry) != CounterExampleMap.end())
            {
              output = TermToConstTermUsingModel(CounterExampleMap[modelentry], ArrayReadFlag);
            }
          else if (ArrayReadFlag)
            {
              //return the array read over a constantindex
              output = modelentry;
            }
          else
            {
              //when all else fails set symbol values to some constant by
              //default. if the variable is queried the second time then add 1
              //to and return the new value.
              ASTNode zero = CreateZeroConst(modelentry.GetValueWidth());
              output = zero;
            }
          break;
        }
      case ITE:
        {
          ASTNode condcompute = ComputeFormulaUsingModel(term[0]);
          if (ASTTrue == condcompute)
            {
              output = TermToConstTermUsingModel(term[1], ArrayReadFlag);
            }
          else if (ASTFalse == condcompute)
            {
              output = TermToConstTermUsingModel(term[2], ArrayReadFlag);
            }
          else
            {
              cerr << "TermToConstTermUsingModel: termITE: value of conditional is wrong: " << condcompute << endl;
              FatalError(" TermToConstTermUsingModel: termITE: cannot compute ITE conditional against model: ", term);
            }
          break;
        }
      default:
        {
          ASTVec c = term.GetChildren();
          ASTVec o;
          for (ASTVec::iterator it = c.begin(), itend = c.end(); it != itend; it++)
            {
              ASTNode ff = TermToConstTermUsingModel(*it, ArrayReadFlag);
              o.push_back(ff);
            }
          output = CreateTerm(k, term.GetValueWidth(), o);
          //output is a CONST expression. compute its value and store it
          //in the CounterExampleMap
          ASTNode oo = BVConstEvaluator(output);
          //the return value
          output = oo;
          break;
        }
      }

    assert(ArrayReadFlag || (BVCONST == output.GetKind()));

    //when this flag is false, we should compute the arrayread to a
    //constant. this constant is stored in the counter_example
    //datastructure
    if (!ArrayReadFlag)
      {
        CounterExampleMap[term] = output;
      }

    //cerr << "Output to TermToConstTermUsingModel: " << output << endl;
    return output;
  } //End of TermToConstTermUsingModel

  //Expands read-over-write by evaluating (readIndex=writeIndex) for
  //every writeindex until, either it evaluates to TRUE or all
  //(readIndex=writeIndex) evaluate to FALSE
  ASTNode BeevMgr::Expand_ReadOverWrite_UsingModel(const ASTNode& term, bool arrayread_flag)
  {
    if (READ != term.GetKind() && WRITE != term[0].GetKind())
      {
        FatalError("RemovesWrites: Input must be a READ over a WRITE", term);
      }

    ASTNode output;
    ASTNodeMap::iterator it1;
    if ((it1 = CounterExampleMap.find(term)) != CounterExampleMap.end())
      {
        ASTNode val = it1->second;
        if (BVCONST != val.GetKind())
          {
            //recursion is fine here. There are two maps that are checked
            //here. One is the substitutionmap. We garuntee that the value
            //of a key in the substitutionmap is always a constant.
            if (term == val)
              {
                FatalError("TermToConstTermUsingModel: The input term is stored as-is "
                           "in the CounterExample: Not ok: ", term);
              }
            return TermToConstTermUsingModel(val, arrayread_flag);
          }
        else
          {
            return val;
          }
      }

    unsigned int width = term.GetValueWidth();
    ASTNode writeA = ASTTrue;
    ASTNode newRead = term;
    ASTNode readIndex = TermToConstTermUsingModel(newRead[1], false);
    //iteratively expand read-over-write, and evaluate against the
    //model at every iteration
    do
      {
        ASTNode write = newRead[0];
        writeA = write[0];
        ASTNode writeIndex = TermToConstTermUsingModel(write[1], false);
        ASTNode writeVal = TermToConstTermUsingModel(write[2], false);

        ASTNode cond = ComputeFormulaUsingModel(CreateSimplifiedEQ(writeIndex, readIndex));
        if (ASTTrue == cond)
          {
            //found the write-value. return it
            output = writeVal;
            CounterExampleMap[term] = output;
            return output;
          }

        newRead = CreateTerm(READ, width, writeA, readIndex);
      } while (READ == newRead.GetKind() && WRITE == newRead[0].GetKind());

    output = TermToConstTermUsingModel(newRead, arrayread_flag);

    //memoize
    CounterExampleMap[term] = output;
    return output;
  } //Exand_ReadOverWrite_To_ITE_UsingModel()

  /* FUNCTION: accepts a non-constant formula, and checks if the
   * formula is ASTTrue or ASTFalse w.r.t to a model
   */
  ASTNode BeevMgr::ComputeFormulaUsingModel(const ASTNode& form)
  {
    ASTNode in = form;
    Kind k = form.GetKind();
    if (!(is_Form_kind(k) && BOOLEAN_TYPE == form.GetType()))
      {
        FatalError(" ComputeConstFormUsingModel: The input is a non-formula: ", form);
      }

    //cerr << "Input to ComputeFormulaUsingModel:" << form << endl;
    ASTNodeMap::iterator it1;
    if ((it1 = ComputeFormulaMap.find(form)) != ComputeFormulaMap.end())
      {
        ASTNode res = it1->second;
        if (ASTTrue == res || ASTFalse == res)
          {
            return res;
          }
        else
          {
            FatalError("ComputeFormulaUsingModel: The value of a formula must be TRUE or FALSE:", form);
          }
      }

    ASTNode t0, t1;
    ASTNode output = ASTFalse;
    switch (k)
      {
      case TRUE:
      case FALSE:
        output = form;
        break;
      case SYMBOL:
        if (BOOLEAN_TYPE != form.GetType())
          FatalError(" ComputeFormulaUsingModel: Non-Boolean variables are not formulas", form);
        if (CounterExampleMap.find(form) != CounterExampleMap.end())
          {
            ASTNode counterexample_val = CounterExampleMap[form];
            if (!VarSeenInTerm(form, counterexample_val))
              {
                output = ComputeFormulaUsingModel(counterexample_val);
              }
            else
              {
                output = counterexample_val;
              }
          }
        else
          output = ASTFalse;
        break;
      case EQ:
      case NEQ:
      case BVLT:
      case BVLE:
      case BVGT:
      case BVGE:
      case BVSLT:
      case BVSLE:
      case BVSGT:
      case BVSGE:
        //convert form[0] into a constant term
        t0 = TermToConstTermUsingModel(form[0], false);
        //convert form[0] into a constant term
        t1 = TermToConstTermUsingModel(form[1], false);
        output = BVConstEvaluator(CreateNode(k, t0, t1));

        //evaluate formula to false if bvdiv execption occurs while
        //counterexample is being checked during refinement.
        if (bvdiv_exception_occured && counterexample_checking_during_refinement)
          {
            output = ASTFalse;
          }
        break;
      case NAND:
        {
          ASTNode o = ASTTrue;
          for (ASTVec::const_iterator it = form.begin(), itend = form.end(); it != itend; it++)
            if (ASTFalse == ComputeFormulaUsingModel(*it))
              {
                o = ASTFalse;
                break;
              }
          if (o == ASTTrue)
            output = ASTFalse;
          else
            output = ASTTrue;
          break;
        }
      case NOR:
        {
          ASTNode o = ASTFalse;
          for (ASTVec::const_iterator it = form.begin(), itend = form.end(); it != itend; it++)
            if (ASTTrue == ComputeFormulaUsingModel(*it))
              {
                o = ASTTrue;
                break;
              }
          if (o == ASTTrue)
            output = ASTFalse;
          else
            output = ASTTrue;
          break;
        }
      case NOT:
        if (ASTTrue == ComputeFormulaUsingModel(form[0]))
          output = ASTFalse;
        else
          output = ASTTrue;
        break;
      case OR:
        for (ASTVec::const_iterator it = form.begin(), itend = form.end(); it != itend; it++)
          if (ASTTrue == ComputeFormulaUsingModel(*it))
            output = ASTTrue;
        break;
      case AND:
        output = ASTTrue;
        for (ASTVec::const_iterator it = form.begin(), itend = form.end(); it != itend; it++)
          {
            if (ASTFalse == ComputeFormulaUsingModel(*it))
              {
                output = ASTFalse;
                break;
              }
          }
        break;
      case XOR:
        t0 = ComputeFormulaUsingModel(form[0]);
        t1 = ComputeFormulaUsingModel(form[1]);
        if ((ASTTrue == t0 && ASTTrue == t1) || (ASTFalse == t0 && ASTFalse == t1))
          output = ASTFalse;
        else
          output = ASTTrue;
        break;
      case IFF:
        t0 = ComputeFormulaUsingModel(form[0]);
        t1 = ComputeFormulaUsingModel(form[1]);
        if ((ASTTrue == t0 && ASTTrue == t1) || (ASTFalse == t0 && ASTFalse == t1))
          output = ASTTrue;
        else
          output = ASTFalse;
        break;
      case IMPLIES:
        t0 = ComputeFormulaUsingModel(form[0]);
        t1 = ComputeFormulaUsingModel(form[1]);
        if ((ASTFalse == t0) || (ASTTrue == t0 && ASTTrue == t1))
          output = ASTTrue;
        else
          output = ASTFalse;
        break;
      case ITE:
        t0 = ComputeFormulaUsingModel(form[0]);
        if (ASTTrue == t0)
          output = ComputeFormulaUsingModel(form[1]);
        else if (ASTFalse == t0)
          output = ComputeFormulaUsingModel(form[2]);
        else
          FatalError("ComputeFormulaUsingModel: ITE: something is wrong with the formula: ", form);
        break;
      default:
        FatalError(" ComputeFormulaUsingModel: the kind has not been implemented", ASTUndefined);
        break;
      }

    //cout << "ComputeFormulaUsingModel output is:" << output << endl;
    ComputeFormulaMap[form] = output;
    return output;
  }

  void BeevMgr::CheckCounterExample(bool t)
  {
    // FIXME:  Code is more useful if enable flags are check OUTSIDE the method.
    // If I want to check a counterexample somewhere, I don't want to have to set
    // the flag in order to make it actualy happen!

    printf("checking counterexample\n");
    if (!check_counterexample_flag)
      {
        return;
      }

    //input is valid, no counterexample to check
    if (ValidFlag)
      return;

    //t is true if SAT solver generated a counterexample, else it is false
    if (!t)
      FatalError("CheckCounterExample: No CounterExample to check", ASTUndefined);
    const ASTVec c = GetAsserts();
    for (ASTVec::const_iterator it = c.begin(), itend = c.end(); it != itend; it++)
      if (ASTFalse == ComputeFormulaUsingModel(*it))
        FatalError("CheckCounterExample:counterexample bogus:"
                   "assert evaluates to FALSE under counterexample: NOT OK", *it);

    if (ASTTrue == ComputeFormulaUsingModel(_current_query))
      FatalError("CheckCounterExample:counterexample bogus:"
                 "query evaluates to TRUE under counterexample: NOT OK", _current_query);
  }

  /* FUNCTION: prints a counterexample for INVALID inputs.  iterate
   * through the CounterExampleMap data structure and print it to
   * stdout
   */
  void BeevMgr::PrintCounterExample(bool t, std::ostream& os)
  {
    //global command-line option
    // FIXME: This should always print the counterexample.  If you want
    // to turn it off, check the switch at the point of call.
    if (!print_counterexample_flag)
      {
        return;
      }

    //input is valid, no counterexample to print
    if (ValidFlag)
      {
        return;
      }

    //if this option is true then print the way dawson wants using a
    //different printer. do not use this printer.
    if (print_arrayval_declaredorder_flag)
      {
        return;
      }

    //t is true if SAT solver generated a counterexample, else it is
    //false
    if (!t)
      {
        cerr << "PrintCounterExample: No CounterExample to print: " << endl;
        return;
      }

    //os << "\nCOUNTEREXAMPLE: \n" << endl;
    ASTNodeMap::iterator it = CounterExampleMap.begin();
    ASTNodeMap::iterator itend = CounterExampleMap.end();
    for (; it != itend; it++)
      {
        ASTNode f = it->first;
        ASTNode se = it->second;

        if (ARRAY_TYPE == se.GetType())
          {
            FatalError("TermToConstTermUsingModel: entry in counterexample is an arraytype. bogus:", se);
          }

        //skip over introduced variables
        if (f.GetKind() == SYMBOL && (_introduced_symbols.find(f) != _introduced_symbols.end()))
          continue;
        if (f.GetKind() == SYMBOL || (f.GetKind() == READ && f[0].GetKind() == SYMBOL && f[1].GetKind() == BVCONST))
          {
            os << "ASSERT( ";
            f.PL_Print(os,0);
            os << " = ";
            if (BITVECTOR_TYPE == se.GetType())
              {
                TermToConstTermUsingModel(se, false).PL_Print(os, 0);
              }
            else
              {
                se.PL_Print(os, 0);
              }
            os << " );" << endl;
          }
      }
    //os << "\nEND OF COUNTEREXAMPLE" << endl;
  } //End of PrintCounterExample

  /* iterate through the CounterExampleMap data structure and print it
   * to stdout. this function prints only the declared array variables
   * IN the ORDER in which they were declared. It also assumes that
   * the variables are of the form 'varname_number'. otherwise it will
   * not print anything. This function was specifically written for
   * Dawson Engler's group (bug finding research group at Stanford)
   */
  void BeevMgr::PrintCounterExample_InOrder(bool t)
  {
    //global command-line option to print counterexample. we do not
    //want both counterexample printers to print at the sametime.
    // FIXME: This should always print the counterexample.  If you want
    // to turn it off, check the switch at the point of call.
    if (print_counterexample_flag)
      return;

    //input is valid, no counterexample to print
    if (ValidFlag)
      return;

    //print if the commandline option is '-q'. allows printing the
    //counterexample in order.
    if (!print_arrayval_declaredorder_flag)
      return;

    //t is true if SAT solver generated a counterexample, else it is
    //false
    if (!t)
      {
        cerr << "PrintCounterExample: No CounterExample to print: " << endl;
        return;
      }

    //vector to store the integer values
    std::vector<int> out_int;
    cout << "% ";
    for (ASTVec::iterator it = _special_print_set.begin(), itend = _special_print_set.end(); it != itend; it++)
      {
        if (ARRAY_TYPE == it->GetType())
          {
            //get the name of the variable
            const char * c = it->GetName();
            std::string ss(c);
            if (!(0 == strncmp(ss.c_str(), "ini_", 4)))
              continue;
            reverse(ss.begin(), ss.end());

            //cout << "debugging: " << ss;
            size_t pos = ss.find('_', 0);
            if (!((0 < pos) && (pos < ss.size())))
              continue;

            //get the associated length
            std::string sss = ss.substr(0, pos);
            reverse(sss.begin(), sss.end());
            int n = atoi(sss.c_str());

            it->PL_Print(cout, 2);
            for (int j = 0; j < n; j++)
              {
                ASTNode index = CreateBVConst(it->GetIndexWidth(), j);
                ASTNode readexpr = CreateTerm(READ, it->GetValueWidth(), *it, index);
                ASTNode val = GetCounterExample(t, readexpr);
                //cout << "ASSERT( ";
                //cout << " = ";
                out_int.push_back(GetUnsignedConst(val));
                //cout << "\n";
              }
          }
      }
    cout << endl;
    for (unsigned int jj = 0; jj < out_int.size(); jj++)
      cout << out_int[jj] << endl;
    cout << endl;
  } //End of PrintCounterExample_InOrder

  /* FUNCTION: queries the CounterExampleMap object with 'expr' and
   * returns the corresponding counterexample value.
   */
  ASTNode BeevMgr::GetCounterExample(bool t, const ASTNode& expr)
  {
    //input is valid, no counterexample to get
    if (ValidFlag)
      return ASTUndefined;

    if (BOOLEAN_TYPE == expr.GetType())
      {
        return ComputeFormulaUsingModel(expr);
      }

    if (BVCONST == expr.GetKind())
      {
        return expr;
      }

    ASTNodeMap::iterator it;
    ASTNode output;
    if ((it = CounterExampleMap.find(expr)) != CounterExampleMap.end())
      output = TermToConstTermUsingModel(CounterExampleMap[expr], false);
    else
      output = CreateZeroConst(expr.GetValueWidth());
    return output;
  } //End of GetCounterExample

  //##################################################
  //##################################################


  void BeevMgr::printCacheStatus()
  {
    cerr << SimplifyMap->size() << endl;
    cerr << SimplifyNegMap->size() << endl;
    cerr << ReferenceCount->size() << endl;
    cerr << TermsAlreadySeenMap.size() << endl;

    cerr << SimplifyMap->bucket_count() << endl;
    cerr << SimplifyNegMap->bucket_count() << endl;
    cerr << ReferenceCount->bucket_count() << endl;
    cerr << TermsAlreadySeenMap.bucket_count() << endl;



  }

  // FIXME:  Don't use numeric codes.  Use an enum type!
  //Acceps a query, calls the SAT solver and generates Valid/InValid.
  //if returned 0 then input is INVALID
  //if returned 1 then input is VALID
  //if returned 2 then ERROR
  int BeevMgr::TopLevelSATAux(const ASTNode& inputasserts)
  {
    ASTNode q = inputasserts;
    ASTNode orig_input = q;
    ASTNodeStats("input asserts and query: ", q);

    ASTNode newq = q;
    //round of substitution, solving, and simplification. ensures that
    //DAG is minimized as much as possibly, and ideally should
    //garuntee that all liketerms in BVPLUSes have been combined.
    BVSolver bvsolver(this);
    SimplifyWrites_InPlace_Flag = false;
    Begin_RemoveWrites = false;
    start_abstracting = false;
    TermsAlreadySeenMap.clear();
    do
      {
        q = newq;
        newq = CreateSubstitutionMap(newq);
        //printf("##################################################\n");
        ASTNodeStats("after pure substitution: ", newq);
        newq = SimplifyFormula_TopLevel(newq, false);
        ASTNodeStats("after simplification: ", newq);
        newq = bvsolver.TopLevelBVSolve(newq);
        ASTNodeStats("after solving: ", newq);
      } while (q != newq);

    ASTNodeStats("Before SimplifyWrites_Inplace begins: ", newq);
    SimplifyWrites_InPlace_Flag = true;
    Begin_RemoveWrites = false;
    start_abstracting = false;
    TermsAlreadySeenMap.clear();
    do
      {
        q = newq;
        newq = CreateSubstitutionMap(newq);
        ASTNodeStats("after pure substitution: ", newq);
        newq = SimplifyFormula_TopLevel(newq, false);
        ASTNodeStats("after simplification: ", newq);
        newq = bvsolver.TopLevelBVSolve(newq);
        ASTNodeStats("after solving: ", newq);
      } while (q != newq);
    ASTNodeStats("After SimplifyWrites_Inplace: ", newq);

    start_abstracting = (arraywrite_refinement_flag) ? true : false;
    SimplifyWrites_InPlace_Flag = false;
    Begin_RemoveWrites = (start_abstracting) ? false : true;
    if (start_abstracting)
      {
        ASTNodeStats("before abstraction round begins: ", newq);
      }

    TermsAlreadySeenMap.clear();
    do
      {
        q = newq;
        //newq = CreateSubstitutionMap(newq);
        //Begin_RemoveWrites = true;
        //ASTNodeStats("after pure substitution: ", newq);
        newq = SimplifyFormula_TopLevel(newq, false);
        //ASTNodeStats("after simplification: ", newq);
        //newq = bvsolver.TopLevelBVSolve(newq);
        //ASTNodeStats("after solving: ", newq);
      } while (q != newq);

    if (start_abstracting)
      {
        ASTNodeStats("After abstraction: ", newq);
      }
    start_abstracting = false;
    SimplifyWrites_InPlace_Flag = false;
    Begin_RemoveWrites = false;

    newq = TransformFormula_TopLevel(newq);
    ASTNodeStats("after transformation: ", newq);
    TermsAlreadySeenMap.clear();

    //if(stats_flag)
    //  printCacheStatus();

    int res;
    //solver instantiated here
    MINISAT::Solver newS;
    //MINISAT::SimpSolver newS;
    //MINISAT::UnsoundSimpSolver newS;
    if (arrayread_refinement_flag)
      {
        counterexample_checking_during_refinement = true;
      }

    res = CallSAT_ResultCheck(newS, newq, orig_input);
    if (2 != res)
      {
        CountersAndStats("print_func_stats");
        return res;
      }

    res = SATBased_ArrayReadRefinement(newS, newq, orig_input);
    if (2 != res)
      {
        CountersAndStats("print_func_stats");
        return res;
      }

    res = SATBased_ArrayWriteRefinement(newS, orig_input);
    if (2 != res)
      {
        CountersAndStats("print_func_stats");
        return res;
      }

    res = SATBased_ArrayReadRefinement(newS, newq, orig_input);
    if (2 != res)
      {
        CountersAndStats("print_func_stats");
        return res;
      }

    FatalError("TopLevelSATAux: reached the end without proper conclusion:"
               "either a divide by zero in the input or a bug in STP");
    //bogus return to make the compiler shut up
    return 2;
  } //End of TopLevelSAT

  /*******************************************************************
   * Helper Functions
   *******************************************************************/
  //FUNCTION: this function accepts a boolvector and returns a BVConst
  ASTNode BeevMgr::BoolVectoBVConst(hash_map<unsigned, bool> * w, unsigned int l)
  {
    unsigned len = w->size();
    if (l < len)
      FatalError("BoolVectorBVConst : "
                 "length of bitvector does not match hash_map size:", ASTUndefined, l);
    std::string cc;
    for (unsigned int jj = 0; jj < l; jj++)
      {
        if ((*w)[jj] == true)
          cc += '1';
        else if ((*w)[jj] == false)
          cc += '0';
        else
          cc += '0';
      }
    return CreateBVConst(cc.c_str(), 2);
  }

  //This function prints the output of the STP solver
  void BeevMgr::PrintOutput(bool true_iff_valid)
  {
    if (print_output_flag)
      {
        if (smtlib_parser_flag)
          {
            if (true_iff_valid && (BEEV::input_status == TO_BE_SATISFIABLE))
              {
                cerr << "Warning. Expected satisfiable, FOUND unsatisfiable" << endl;
              }
            else if (!true_iff_valid && (BEEV::input_status == TO_BE_UNSATISFIABLE))
              {
                cerr << "Warning. Expected unsatisfiable, FOUND satisfiable" << endl;
              }
          }
      }

    if (true_iff_valid)
      {
        ValidFlag = true;
        if (print_output_flag)
          {
            if (smtlib_parser_flag)
              cout << "unsat\n";
            else
              cout << "Valid.\n";
          }
      }
    else
      {
        ValidFlag = false;
        if (print_output_flag)
          {
            if (smtlib_parser_flag)
              cout << "sat\n";
            else
              cout << "Invalid.\n";
          }
      }
  }
}
; //end of namespace BEEV
