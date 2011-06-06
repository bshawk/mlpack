/**
 * @file loglik.cc
 *
 * This file contains the program to compute log-likelihood of sequences
 * according to a Hidden Markov  Model.
 *
 * Usage:
 *   loglik --type=TYPE --profile=PROFILE [OPTIONS]
 * See the usage() function for complete option list
 */

#include <fastlib/fastlib.h>
#include <fastlib/fx/io.h>
#include "support.h"
#include "discreteHMM.h"
#include "gaussianHMM.h"
#include "mixgaussHMM.h"
#include "mixtureDST.h"

using namespace hmm_support;

success_t loglik_discrete();
success_t loglik_gaussian();
success_t loglik_mixture();
void usage();

/*const fx_entry_doc hmm_loglik_main_entries[] = {
  {"type", FX_REQUIRED, FX_STR, NULL,
   "  HMM type : discrete | gaussian | mixture.\n"},
  {"profile", FX_REQUIRED, FX_STR, NULL,
   "  A file containing HMM profile.\n"},
  {"seqfile", FX_PARAM, FX_STR, NULL,
   "  Output file for the data sequences.\n"},
  {"logfile", FX_PARAM, FX_STR, NULL,
   "  Output file for the computed log-likelihood of the sequences.\n"},
  FX_ENTRY_DOC_DONE
};*/

PARAM_STRING_REQ("type", "HMM type : discrete | gaussian | mixture.", "hmm");
PARAM_STRING_REQ("profile", "A file containing HMM profile.", "hmm");
PARAM_STRING("seqfile", "Outputfile for the datasequences.", 
	"hmm", "seq.mix.out");
PARAM_STRING("logfile", 
	"Output file for the computed log-likelihood of the sequences.", 
	"hmm", "log.mix.out");

PARAM_MODULE("hmm", "This is a program computing log-likelihood of data \nsequences from HMM models.");
/* const fx_submodule_doc hmm_loglik_main_submodules[] = {
  FX_SUBMODULE_DOC_DONE
}; */

/* const fx_module_doc hmm_loglik_main_doc = {
  hmm_loglik_main_entries, hmm_loglik_main_submodules,
  "This is a program computing log-likelihood of data sequences \n"
  "from HMM models.\n"
}; */

using namespace mlpack;

int main(int argc, char* argv[]) {
  IO::ParseCommandLine(argc, argv);

  success_t s = SUCCESS_PASS;
  if (IO::CheckValue("hmm/type")) {
    const char* type = IO::GetValue<std::string>("hmm/type").c_str();
    if (strcmp(type, "discrete")==0)
      s = loglik_discrete();
    else if (strcmp(type, "gaussian")==0) 
      s = loglik_gaussian();
    else if (strcmp(type, "mixture")==0) 
      s = loglik_mixture();
    else {
      IO::Info << "Unrecognized type: must be: discrete | gaussian | mixture !!!";
      s = SUCCESS_FAIL;
    }
  }
  else {
    IO::Info << "Unrecognized type: must be: discrete | gaussian | mixture  !!!";
    s = SUCCESS_FAIL;
  }
  if (!PASSED(s)) usage();
}

void usage() {
  IO::Warn << "\n" << std::endl;
  IO::Warn << "Usage:\n" << std::endl;
  IO::Warn << "  loglik --type=={discrete|gaussian|mixture} OPTIONS" << std::endl;
  IO::Warn << "[OPTIONS]" << std::endl;
  IO::Warn << "  --profile==file   : file contains HMM profile" << std::endl;
  IO::Warn << "  --seqfile==file   : file contains input sequences" << std::endl;
  IO::Warn << "  --logfile==file   : output file for log-likelihood of the sequences" << std::endl;
}

success_t loglik_mixture() {
  if (!IO::CheckValue("hmm/profile")) {
    IO::Warn << "--profile must be defined." << std::endl;
    return SUCCESS_FAIL;
  }
  const char* profile = IO::GetValue<std::string>("hmm/profile").c_str();
  const char* seqin = IO::GetValue<std::string>("hmm/seqfile").c_str(); 
  const char* logout = IO::GetValue<std::string>("hmm/logfile").c_str(); 

  MixtureofGaussianHMM hmm;
  hmm.InitFromFile(profile);

  std::vector<arma::mat> seqs;
  load_matrix_list(seqin, seqs);

  TextWriter w_log;
  if (!PASSED(w_log.Open(logout))) {
    IO::Warn << "Couldn't open '" << logout << "' for writing." << std::endl;
    return SUCCESS_FAIL;
  }

  std::vector<double> list_loglik;
  hmm.ComputeLogLikelihood(seqs, list_loglik);

  for (index_t i = 0; i < seqs.size(); i++)
    w_log.Printf("%f\n", list_loglik[i]);
  
  return SUCCESS_PASS;
}

success_t loglik_gaussian() {
  if (!IO::CheckValue("hmm/profile")) {
    IO::Warn << "--profile must be defined." << std::endl;
    return SUCCESS_FAIL;
  }
  const char* profile = IO::GetValue<std::string>("hmm/profile").c_str();
  const char* seqin = IO::GetValue<std::string>("hmm/seqfile").c_str(); 
  const char* logout = IO::GetValue<std::string>("hmm/logfile").c_str();

  GaussianHMM hmm;
  hmm.InitFromFile(profile);

  std::vector<arma::mat> seqs;
  load_matrix_list(seqin, seqs);

  TextWriter w_log;
  if (!PASSED(w_log.Open(logout))) {
    IO::Warn << "Couldn't open '"<< logout <<"' for writing." << std::endl;
    return SUCCESS_FAIL;
  }

  std::vector<double> list_loglik;
  hmm.ComputeLogLikelihood(seqs, list_loglik);

  for (index_t i = 0; i < seqs.size(); i++)
    w_log.Printf("%f\n", list_loglik[i]);
  
  return SUCCESS_PASS;
}

success_t loglik_discrete() {
  if (!IO::CheckValue("hmm/profile")) {
    IO::Warn << "--profile must be defined." << std::endl;
    return SUCCESS_FAIL;
  }
  const char* profile = IO::GetValue<std::string>("hmm/profile").c_str();
  const char* seqin = IO::GetValue<std::string>("hmm/seqfile").c_str(); 
  const char* logout = IO::GetValue<std::string>("hmm/logfile").c_str();

  DiscreteHMM hmm;
  hmm.InitFromFile(profile);

  std::vector<arma::vec> seqs;
  load_vector_list(seqin, seqs);

  TextWriter w_log;
  if (!PASSED(w_log.Open(logout))) {
    IO::Warn << "Couldn't open '"<< logout <<"' for writing." << std::endl;
    return SUCCESS_FAIL;
  }

  std::vector<double> list_loglik;
  hmm.ComputeLogLikelihood(seqs, list_loglik);

  for (index_t i = 0; i < seqs.size(); i++)
    w_log.Printf("%f\n", list_loglik[i]);

  return SUCCESS_PASS;
}
