#include <vector>
#include <boost/intrusive/slist.hpp>
#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/alignment_of.hpp>


//the hook must be unlinked before the pool destructor is called.
//the destructor of a safe-link intrusive container does unlink in its destructor.


template<class T,class List,class ValueTraits,class Hook,std::size_t BlockSize>
class intrusive_pool_base{
private:
	typedef typename List::value_type value_type;
public:
	intrusive_pool_base() : first_block(0){}
	T *new_(){
		if(this->unused.empty()) this->allocate_block();
		value_type &val=this->unused.front();
		this->unused.pop_front();
		val.~Hook();
		T &t=ValueTraits::from_value(val);
		new (&t) T;
		return &t;
	}
	void delete_(T *e){
		e->~T();
		value_type &val=ValueTraits::to_value(*e);
		construct_hook(val);
		this->unused.push_front(val);
	}
	~intrusive_pool_base(){
		if(this->first_block){
			destruct_block(this->first_block);
		}
		for(typename more_blocks_type::iterator it=this->more_blocks.begin();it != this->more_blocks.end();++it){
			destruct_block(*it);
		}
		this->unused.clear();
		delete [] this->first_block;
		for(typename more_blocks_type::iterator it=this->more_blocks.begin();it != this->more_blocks.end();++it){
			delete [] *it;
		}
	}
private:
	static std::size_t const alignment=boost::alignment_of<value_type>::value;
	BOOST_STATIC_ASSERT(sizeof(Hook) <= alignment);

	static void construct_hook(value_type &e){
		//this would crash if the hook is in a virtual base class, because e is not constructed.
		//but boost intrusive fails at compile time in this case
		Hook &hook=e; 
		new (&hook) Hook;
	}
	static value_type *element_ptr(char *block,std::size_t index){
		return reinterpret_cast<value_type *>(block + index * alignment);
	}
	static void destruct_block(char *block){
		for(std::size_t c=0;c<BlockSize;++c){
			value_type *ele=element_ptr(block,c);
			Hook *hook=ele;
			if(!hook->is_linked()){ //is not part of this->unused
				ValueTraits::from_value(*ele).~T();
			}
		}
	}

	void allocate_block(){
		char *newblock=new char[BlockSize * alignment];
		if(this->first_block) this->more_blocks.push_back(newblock);
		else this->first_block=newblock;

		for(std::size_t c=0;c<BlockSize;++c){
			value_type *ele=element_ptr(newblock,c);
			construct_hook(*ele);
			this->unused.push_front(*ele);
		}
	}

	char *first_block; //no vector allocation if only 1 block is used
	typedef std::vector<char *> more_blocks_type;
	more_blocks_type more_blocks;
	
	List unused;
};


template<class T>
struct identity_traits{
	static T &to_value(T &t){ return t; }
	static T &from_value(T &t){ return t; }
};

template<class T>
struct hooked : intrusive::slist_base_hook<>{
	typedef intrusive::slist_base_hook<> base_type;
	T t;
};

template<class T>
struct hooked_traits{
	static T &from_value(hooked<T> &h){
		T &t=h.t;
		BOOST_ASSERT(&to_value(t) == &h);
		return t;
	}
	static hooked<T> &to_value(T &t){
		char *tmp=reinterpret_cast<char *>(&t);
		return *reinterpret_cast<hooked<T> *>(tmp - sizeof(typename hooked<T>::base_type));
	}
};

template<class T,class Hook,std::size_t BlockSize>
class slist_pool
	: public intrusive_pool_base
		< T
		, intrusive::slist<T,intrusive::base_hook<Hook>,intrusive::constant_time_size<false> >
		, identity_traits<T>
		, Hook
		, BlockSize
		>{};

template<class T,class Hook,std::size_t BlockSize>
class list_pool
	: public intrusive_pool_base
		< T
		, intrusive::list<T,intrusive::base_hook<Hook>,intrusive::constant_time_size<false> >
		, identity_traits<T>
		, Hook
		, BlockSize
		>{};

template<class T,std::size_t BlockSize>
class pool
	: public intrusive_pool_base
		< T
		, intrusive::slist<hooked<T>,intrusive::base_hook<intrusive::slist_base_hook<> >,intrusive::constant_time_size<false> >
		, hooked_traits<T>
		, intrusive::slist_base_hook<>
		, BlockSize
		>{};



