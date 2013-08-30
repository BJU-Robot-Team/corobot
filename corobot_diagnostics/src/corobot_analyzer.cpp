/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/


#include "corobot_diagnostics/corobot_analyzer.h"


PLUGINLIB_EXPORT_CLASS(diagnostic_aggregator::CorobotAnalyzer, 
                        diagnostic_aggregator::Analyzer)

using namespace diagnostic_aggregator;
using namespace std;

CorobotAnalyzer::CorobotAnalyzer() { }

bool CorobotAnalyzer::init(const string base_path, const ros::NodeHandle &n)
{ 
  string nice_name;
  if (!n.getParam("path", nice_name))
  {
    ROS_ERROR("GenericAnalyzer was not given parameter \"path\". Namepspace: %s",
              n.getNamespace().c_str());
    return false;
  }

  XmlRpc::XmlRpcValue findRemove;
  if (n.getParam("find_and_remove_prefix", findRemove))
  {
    vector<string> output;
    getParamVals(findRemove, output);
    chaff_ = output;
    startswith_ = output;
  }
  
  XmlRpc::XmlRpcValue removes;
  if (n.getParam("remove_prefix", removes))
    getParamVals(removes, chaff_);
    
  XmlRpc::XmlRpcValue startswith;
  if (n.getParam("startswith", startswith))
    getParamVals(startswith, startswith_);

  XmlRpc::XmlRpcValue name_val;
  if (n.getParam("name", name_val))
    getParamVals(name_val, name_);

  XmlRpc::XmlRpcValue contains;
  if (n.getParam("contains", contains))
    getParamVals(contains, contains_);

  XmlRpc::XmlRpcValue expected;
  if (n.getParam("expected", expected))
  {
    getParamVals(expected, expected_);
    for (unsigned int i = 0; i < expected_.size(); ++i)
    {
      boost::shared_ptr<StatusItem> item(new StatusItem(expected_[i]));
      addItem(expected_[i], item);
    }
 }
 
  XmlRpc::XmlRpcValue regexes;
  if (n.getParam("regex", regexes))
  {
    vector<string> regex_strs;
    getParamVals(regexes, regex_strs);
  
    for (unsigned int i = 0; i < regex_strs.size(); ++i)
    {
      try
      {
        boost::regex re(regex_strs[i]);
        regex_.push_back(re);
      }
      catch (boost::regex_error& e)
      {
        ROS_ERROR("Attempted to make regex from %s. Caught exception, ignoring value. Exception: %s", 
                 regex_strs[i].c_str(), e.what());
      }
    }
  }

  if (startswith_.size() == 0 && name_.size() == 0 && 
      contains_.size() == 0 && expected_.size() == 0 && regex_.size() == 0)
  {
    ROS_ERROR("CorobotAnalyzer was not initialized with any way of checking diagnostics. Name: %s, namespace: %s", nice_name.c_str(), n.getNamespace().c_str());
    return false;
  }
  
  double timeout;
  int num_items_expected;
  bool discard_stale;
  n.param("timeout", timeout, 5.0);   // Timeout for stale
  n.param("num_items", num_items_expected, -1); // Number of items must match this
  n.param("discard_stale", discard_stale, false);
  
  ros::NodeHandle nh;
  
  newError_pub = nh.advertise<std_msgs::String>("new_diagnostic_error", 100);
  removeError_pub = nh.advertise<std_msgs::String>("remove_diagnostic_error", 100);
  isError = false;
  
  string my_path;
  if (base_path == "/")
    my_path = nice_name;
  else
    my_path = base_path + "/" + nice_name;

  if (my_path.find("/") != 0)
    my_path = "/" + my_path;


  return GenericAnalyzerBase::init(my_path, nice_name, 
                                   timeout, num_items_expected, discard_stale);
}

CorobotAnalyzer::~CorobotAnalyzer() 
{ 

}


bool CorobotAnalyzer::match(const string name)
{
  boost::cmatch what;
  for (unsigned int i = 0; i < regex_.size(); ++i)
  {
    if (boost::regex_match(name.c_str(), what, regex_[i]))
      return true;
  }
  
  for (unsigned int i = 0; i < expected_.size(); ++i)
  {
    if (name == expected_[i])
      return true;
  }
  
  for (unsigned int i = 0; i < name_.size(); ++i)
  {
    if (name == name_[i])
      return true;
  }
  
  for (unsigned int i = 0; i < startswith_.size(); ++i)
  {
    if (name.find(startswith_[i]) == 0)
      return true;
  }
  
  for (unsigned int i = 0; i < contains_.size(); ++i)
  {
    if (name.find(contains_[i]) != string::npos)
      return true;
  }
  
  return false;
}

vector<boost::shared_ptr<diagnostic_msgs::DiagnosticStatus> > CorobotAnalyzer::report()
{
  vector<boost::shared_ptr<diagnostic_msgs::DiagnosticStatus> > processed = GenericAnalyzerBase::report();

  // Check and make sure our expected names haven't been removed ...
  vector<string> expected_names_missing;
  bool has_name = false;
  bool all_stale = true;
  
  for (unsigned int i = 0; i < expected_.size(); ++i)
  {
    has_name = false;
    for (unsigned int j = 0; j < processed.size(); ++j)
    {
      if (!processed[j]->level == 3)
        all_stale = false;

      size_t last_slash = processed[j]->name.rfind("/");
      string nice_name = processed[j]->name.substr(last_slash + 1);
      if (nice_name == expected_[i] || nice_name == getOutputName(expected_[i]))
      {
        has_name = true;
        break;
      }

      // Remove chaff, check names
      for (unsigned int k = 0; k < chaff_.size(); ++k)
      {     
        if (nice_name == removeLeadingNameChaff(expected_[i], chaff_[k]))
        {
          has_name = true;
          break;
        }
      }

    }
    if (!has_name)
      expected_names_missing.push_back(expected_[i]);
  }  
  
  // Add missing names to header ...
  for (unsigned int i = 0; i < expected_names_missing.size(); ++i)
  {
    boost::shared_ptr<StatusItem> item(new StatusItem(expected_names_missing[i]));
    processed.push_back(item->toStatusMsg(path_, true));
  } 

  // Check that all processed items aren't stale
  for (unsigned int j = 0; j < processed.size(); ++j)
  {
    if (processed[j]->level != 3)
      all_stale = false;

    for (unsigned int k = 0; k < processed[j]->values.size(); ++k)
    {
      if(processed[j]->values[k].key == "Recommendation")
	  {
	    error = processed[j]->values[k].value;
	    isError = true;
	    std_msgs::String msg;
	    msg.data = error;
	    if(newError_pub)
	        newError_pub.publish(msg);
	    
	  }
    }
    if (processed[j]-> level <= 1 && isError == true)
    {
	    std_msgs::String msg;
	    msg.data = error;
	    if(removeError_pub)
	        removeError_pub.publish(msg);
	    isError = false;
	}
	    
        
  }

  for (unsigned int j = 0; j < processed.size(); ++j)
  {
    // Remove all leading name chaff
    for (unsigned int i = 0; i < chaff_.size(); ++i)
      processed[j]->name = removeLeadingNameChaff(processed[j]->name, chaff_[i]);

    // If we're missing any items, set the header status to error or stale
    if (expected_names_missing.size() > 0 && processed[j]->name == path_)
    {
      if (!all_stale)
      {
        processed[j]->level = 2;
        processed[j]->message = "Error";
      }
      else
      {
        processed[j]->level = 3;
        processed[j]->message = "All Stale";
      }

      // Add all missing items to header item
      for (unsigned int k = 0; k < expected_names_missing.size(); ++k)
      {
        diagnostic_msgs::KeyValue kv;
        kv.key = expected_names_missing[k];
        kv.value = "Missing";
        processed[j]->values.push_back(kv);
      }
    }
  }
  
  return processed;
}

