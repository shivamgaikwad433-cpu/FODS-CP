#ifndef NETWORK_FAILURE_ANALYZER_NETWORK_H
#define NETWORK_FAILURE_ANALYZER_NETWORK_H

#include <algorithm>
#include <climits>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Reusable graph engine. All route, reachability, component and critical-link
// calculations run here; the REST layer only serializes these results.
class Network {
public:
    struct Device { std::string id, name, type; bool down; };
    struct Connection { std::string id, source, target; int latency; bool down; };
    struct Path { std::vector<std::string> devices; int latency; };
    struct Critical { std::vector<std::string> devices; std::vector<std::string> connections; };
private:
    std::vector<Device> devices_;
    std::vector<Connection> links_;
    std::unordered_map<std::string, int> index_;
    std::vector<std::vector<std::pair<int,int> > > adj_;
    std::vector<std::string> events_;
    int nextLink_;
    int find(const std::string& id) const { std::unordered_map<std::string,int>::const_iterator i=index_.find(id); return i==index_.end()?-1:i->second; }
    int linkIndex(const std::string& id) const { for(size_t i=0;i<links_.size();++i) if(links_[i].id==id)return (int)i; return -1; }
    bool working(int i) const { return !devices_[i].down; }
    void dfs(int u,std::vector<bool>& seen,std::vector<int>& group) const { seen[u]=true;group.push_back(u);for(size_t k=0;k<adj_[u].size();++k){int v=adj_[u][k].first,li=adj_[u][k].second;if(!seen[v]&&working(v)&&!links_[li].down)dfs(v,seen,group);} }
    void dijkstra(int s,std::vector<int>& d,std::vector<int>& p) const { d.assign(devices_.size(),INT_MAX);p.assign(devices_.size(),-1);typedef std::pair<int,int> Q;std::priority_queue<Q,std::vector<Q>,std::greater<Q> > q;d[s]=0;q.push(Q(0,s));while(!q.empty()){Q x=q.top();q.pop();int u=x.second;if(x.first!=d[u])continue;for(size_t k=0;k<adj_[u].size();++k){int v=adj_[u][k].first,li=adj_[u][k].second;if(!working(v)||links_[li].down)continue;int nd=d[u]+links_[li].latency;if(nd<d[v]){d[v]=nd;p[v]=u;q.push(Q(nd,v));}}} }
    void paths(int u,int t,std::vector<bool>& seen,std::vector<int>& path,int cost,std::vector<Path>& out)const{if(out.size()>=1000)return;if(u==t){Path x;x.latency=cost;for(size_t i=0;i<path.size();++i)x.devices.push_back(devices_[path[i]].id);out.push_back(x);return;}seen[u]=true;for(size_t k=0;k<adj_[u].size();++k){int v=adj_[u][k].first,li=adj_[u][k].second;if(!seen[v]&&working(v)&&!links_[li].down){path.push_back(v);paths(v,t,seen,path,cost+links_[li].latency,out);path.pop_back();}}seen[u]=false;}
    void tarjan(int u,int parent,int& time,std::vector<int>& disc,std::vector<int>& low,std::vector<bool>& ap,std::vector<int>& bridges) const { disc[u]=low[u]=++time;int children=0;for(size_t k=0;k<adj_[u].size();++k){int v=adj_[u][k].first,li=adj_[u][k].second;if(links_[li].down||!working(v))continue;if(v==parent)continue;if(disc[v])low[u]=std::min(low[u],disc[v]);else{++children;tarjan(v,u,time,disc,low,ap,bridges);low[u]=std::min(low[u],low[v]);if(low[v]>disc[u])bridges.push_back(li);if(parent!=-1&&low[v]>=disc[u])ap[u]=true;}}if(parent==-1&&children>1)ap[u]=true; }
public:
    Network():nextLink_(1){}
    const std::vector<Device>& devices()const{return devices_;} const std::vector<Connection>& connections()const{return links_;} const std::vector<std::string>& events()const{return events_;}
    bool addDevice(const std::string& id,const std::string& name,const std::string& type){if(id.empty()||name.empty()||type.empty()||index_.count(id))return false;index_[id]=(int)devices_.size();devices_.push_back(Device{id,name,type,false});adj_.push_back(std::vector<std::pair<int,int> >());events_.push_back("Device added: "+name);return true;}
    bool editDevice(const std::string& id,const std::string& name,const std::string& type){int i=find(id);if(i<0||name.empty()||type.empty())return false;devices_[i].name=name;devices_[i].type=type;return true;}
    bool deleteDevice(const std::string& id){int i=find(id);if(i<0)return false;std::vector<Connection> keep;for(size_t k=0;k<links_.size();++k)if(links_[k].source!=id&&links_[k].target!=id)keep.push_back(links_[k]);devices_.erase(devices_.begin()+i);index_.clear();adj_.assign(devices_.size(),std::vector<std::pair<int,int> >());for(size_t k=0;k<devices_.size();++k)index_[devices_[k].id]=(int)k;links_=keep;for(size_t k=0;k<links_.size();++k){int u=find(links_[k].source),v=find(links_[k].target);adj_[u].push_back(std::make_pair(v,(int)k));adj_[v].push_back(std::make_pair(u,(int)k));}events_.push_back("Device deleted: "+id);return true;}
    bool addConnection(const std::string& id,const std::string& a,const std::string& b,int latency){int u=find(a),v=find(b);if(id.empty()||u<0||v<0||u==v||latency<0||linkIndex(id)>=0)return false;for(size_t k=0;k<links_.size();++k)if((links_[k].source==a&&links_[k].target==b)||(links_[k].source==b&&links_[k].target==a))return false;int li=(int)links_.size();links_.push_back(Connection{id,a,b,latency,false});adj_[u].push_back(std::make_pair(v,li));adj_[v].push_back(std::make_pair(u,li));return true;}
    bool deleteConnection(const std::string& id){int li=linkIndex(id);if(li<0)return false;std::string a=links_[li].source,b=links_[li].target;int u=find(a),v=find(b);for(int z=0;z<2;++z){std::vector<std::pair<int,int> >& row=adj_[z? v:u];row.erase(std::remove_if(row.begin(),row.end(),[li](const std::pair<int,int>& x){return x.second==li;}),row.end());}links_.erase(links_.begin()+li);for(size_t i=0;i<adj_.size();++i)for(size_t k=0;k<adj_[i].size();++k)if(adj_[i][k].second>li)--adj_[i][k].second;return true;}
    bool editConnection(const std::string& id,int latency){int li=linkIndex(id);if(li<0||latency<0)return false;links_[li].latency=latency;return true;}
    bool failDevice(const std::string& id){int i=find(id);if(i<0)return false;if(!devices_[i].down){devices_[i].down=true;events_.push_back("Device failed: "+devices_[i].name);}return true;}
    bool restoreDevice(const std::string& id){int i=find(id);if(i<0||!devices_[i].down)return false;devices_[i].down=false;events_.push_back("Device restored: "+devices_[i].name);return true;}
    bool failConnection(const std::string& id){int i=linkIndex(id);if(i<0)return false;if(!links_[i].down){links_[i].down=true;events_.push_back("Connection failed: "+id);}return true;}
    bool restoreConnection(const std::string& id){int i=linkIndex(id);if(i<0||!links_[i].down)return false;links_[i].down=false;events_.push_back("Connection restored: "+id);return true;}
    void restoreAll(){for(size_t i=0;i<devices_.size();++i)devices_[i].down=false;for(size_t i=0;i<links_.size();++i)links_[i].down=false;events_.push_back("All failures restored");}
    int components()const{std::vector<bool>s(devices_.size(),false);std::vector<int>g;int n=0;for(size_t i=0;i<devices_.size();++i)if(working((int)i)&&!s[i]){dfs((int)i,s,g);g.clear();++n;}return n;}
    bool shortestPath(const std::string& a,const std::string& b,Path& out)const{int s=find(a),t=find(b);if(s<0||t<0||!working(s)||!working(t))return false;std::vector<int>d,p;dijkstra(s,d,p);if(d[t]==INT_MAX)return false;out.devices.clear();for(int v=t;v!=-1;v=p[v])out.devices.push_back(devices_[v].id);std::reverse(out.devices.begin(),out.devices.end());out.latency=d[t];return true;}
    Critical critical()const{Critical c;int tm=0;std::vector<int>d(devices_.size(),0),lo(devices_.size(),0),br;std::vector<bool>ap(devices_.size(),false);for(size_t i=0;i<devices_.size();++i)if(working((int)i)&&!d[i])tarjan((int)i,-1,tm,d,lo,ap,br);for(size_t i=0;i<ap.size();++i)if(ap[i])c.devices.push_back(devices_[i].id);for(size_t i=0;i<br.size();++i)c.connections.push_back(links_[br[i]].id);return c;}
    std::vector<Path> alternativePaths(const std::string&a,const std::string&b)const{std::vector<Path> out;int s=find(a),t=find(b);if(s<0||t<0||!working(s)||!working(t))return out;std::vector<bool>seen(devices_.size(),false);std::vector<int>path(1,s);paths(s,t,seen,path,0,out);std::sort(out.begin(),out.end(),[](const Path&x,const Path&y){return x.latency<y.latency;});if(out.size()>5)out.resize(5);return out;}
    bool reachable(const std::string&source,std::vector<std::string>&up,std::vector<std::string>&cut)const{int s=find(source);up.clear();cut.clear();if(s<0||!working(s))return false;std::vector<bool>seen(devices_.size(),false);std::queue<int>q;q.push(s);seen[s]=true;while(!q.empty()){int u=q.front();q.pop();up.push_back(devices_[u].id);for(size_t k=0;k<adj_[u].size();++k){int v=adj_[u][k].first,li=adj_[u][k].second;if(!seen[v]&&working(v)&&!links_[li].down){seen[v]=true;q.push(v);}}}for(size_t i=0;i<devices_.size();++i)if(!seen[i]&&working((int)i))cut.push_back(devices_[i].id);return true;}
    void load(const std::vector<Device>& d,const std::vector<Connection>& c,const std::vector<std::string>& e){devices_.clear();links_.clear();adj_.clear();index_.clear();events_=e;nextLink_=1;for(size_t i=0;i<d.size();++i){devices_.push_back(d[i]);index_[d[i].id]=(int)i;adj_.push_back(std::vector<std::pair<int,int> >());}for(size_t i=0;i<c.size();++i){int u=find(c[i].source),v=find(c[i].target);if(u<0||v<0)continue;int li=(int)links_.size();links_.push_back(c[i]);adj_[u].push_back(std::make_pair(v,li));adj_[v].push_back(std::make_pair(u,li));}}
};
#endif
